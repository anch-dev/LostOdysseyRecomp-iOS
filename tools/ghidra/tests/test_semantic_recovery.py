"""Focused tests for the shared pinned-body recovery runner (no native build)."""

from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import semantic_batch  # noqa: E402
import semantic_recovery as recovery  # noqa: E402


FIRST = 'PPC_FUNC_IMPL(__imp__sub_80000000) {\n\tPPC_FUNC_PROLOGUE();\n\t// bl 0x80000020\n\tsub_80000020(ctx, base);\n\t// bne 0x80000010\n\tif (!ctx.cr0.eq) goto loc_80000010;\nloc_80000010:\n\t// blr \n\treturn;\n}'
SECOND = 'PPC_FUNC_IMPL(__imp__sub_80000020) {\n\tPPC_FUNC_PROLOGUE();\n\t// bl 0x80000040\n\t__imp__NativeThing(ctx, base);\n\t// blr \n\treturn;\n}'


class SemanticRecoveryTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.ppc = self.root / "ppc"
        self.ppc.mkdir()
        (self.ppc / "ppc_recomp.0.cpp").write_text(FIRST + "\n" + SECOND + "\n",
                                                      encoding="utf-8")
        self.manifest = self.root / "family.json"
        self.entries = [
            {"address": "80000000", "source": "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp",
             "source_line": 1, "instructions": ["bl 0x80000020", "bne 0x80000010", "blr "],
             "direct_calls": ["80000020"],
             "cfg": {"labels": ["80000010"],
                     "branches": ["bl 0x80000020", "bne 0x80000010", "blr "]},
             "translated_body": FIRST},
            {"address": "80000020", "source": "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp",
             "source_line": FIRST.count("\n") + 2,
             "instructions": ["bl 0x80000040", "blr "], "direct_calls": [],
             "native_calls": ["NativeThing"], "cfg": {"labels": [],
                 "branches": ["bl 0x80000040", "blr "]},
             "translated_body": SECOND},
        ]
        self.save()

    def save(self) -> None:
        self.manifest.write_text(json.dumps({"schema": "synthetic-v1", "entries": self.entries}),
                                 encoding="utf-8")

    def test_two_bodies_in_one_file_read_once(self) -> None:
        original = Path.read_text
        reads = []

        def counted(path, *args, **kwargs):
            if path == self.ppc / "ppc_recomp.0.cpp":
                reads.append(path)
            return original(path, *args, **kwargs)

        with patch.object(Path, "read_text", counted):
            checked = recovery.validate_manifests([self.manifest], self.ppc)
        self.assertEqual(len(reads), 1)
        self.assertEqual(checked[0]["entries"], ["80000000", "80000020"])
        self.assertEqual(checked[0]["original_cpp"], (FIRST + "\n" + SECOND).encode())

    def test_source_cache_shared_across_manifests(self) -> None:
        first = self.root / "first.json"
        second = self.root / "second.json"
        first.write_text(json.dumps({"entries": [self.entries[0]]}), encoding="utf-8")
        second.write_text(json.dumps({"entries": [self.entries[1]]}), encoding="utf-8")
        original = Path.read_text
        reads = []

        def counted(path, *args, **kwargs):
            if path == self.ppc / "ppc_recomp.0.cpp":
                reads.append(path)
            return original(path, *args, **kwargs)

        cache = {}
        with patch.object(Path, "read_text", counted):
            recovery.validate_manifests([first], self.ppc, cache)
            recovery.validate_manifests([second], self.ppc, cache)
        self.assertEqual(len(reads), 1)

    def test_changed_label_rejected_by_complete_body(self) -> None:
        source = self.ppc / "ppc_recomp.0.cpp"
        source.write_text(source.read_text().replace("loc_80000010:", "loc_80000014:"),
                          encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "complete translated body changed"):
            recovery.validate_manifests([self.manifest], self.ppc)

    def test_changed_call_rejected(self) -> None:
        self.entries[0]["direct_calls"] = ["80000024"]
        self.save()
        with self.assertRaisesRegex(ValueError, "direct call target changed"):
            recovery.validate_manifests([self.manifest], self.ppc)

    def test_duplicate_address_rejected(self) -> None:
        self.entries[1]["address"] = "80000000"
        self.save()
        with self.assertRaisesRegex(ValueError, "duplicate entry address"):
            recovery.validate_manifests([self.manifest], self.ppc)

    def test_bad_source_path_rejected(self) -> None:
        self.entries[0]["source"] = "../ppc_recomp.0.cpp"
        self.save()
        with self.assertRaisesRegex(ValueError, "unexpected PPC source path"):
            recovery.validate_manifests([self.manifest], self.ppc)

    def test_old_pass_receipts_replaced_before_failed_validation(self) -> None:
        self.entries[0]["translated_body"] = FIRST + "\n// drift"
        self.save()
        harness = self.root / "oracle.cpp"
        harness.write_text("", encoding="utf-8")
        batch = self.root / "batch.json"
        batch.write_text(json.dumps({"families": [{"name": "synthetic", "manifest":
            str(self.manifest), "harness": str(harness), "prelude": "", "sources": []}]}),
            encoding="utf-8")
        output = self.root / "result"
        output.mkdir()
        for name in ("semantic-recovery-result.json", "synthetic-result.json"):
            (output / name).write_text('{"status":"passed"}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "complete translated body changed"):
            recovery.run_batch(batch, output, self.ppc)
        overall = json.loads((output / "semantic-recovery-result.json").read_text())
        family = json.loads((output / "synthetic-result.json").read_text())
        self.assertEqual(overall["status"], "failed")
        self.assertEqual(family["status"], "failed")
        self.assertIn("complete translated body changed", family["error"])

    def test_library_build_rejects_other_checkout_before_compiler_setup(self) -> None:
        build = self.root / "build"
        build.mkdir()
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={self.root / 'other'}\n", encoding="utf-8")
        with patch.object(recovery, "compiler_environment") as setup:
            with self.assertRaisesRegex(ValueError, "source differs"):
                recovery._build_library(build, self.root)
        setup.assert_not_called()

    def test_library_build_uses_native_environment_and_finite_path(self) -> None:
        build = self.root / "build"
        build.mkdir()
        (build / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={recovery.ROOT}\n"
            "CMAKE_BUILD_TYPE:STRING=Release\n", encoding="utf-8")
        library = (build / "LostOdysseyRecompSemantics" / "Release" /
                   "LostOdysseyRecompSemantics.lib")
        library.parent.mkdir(parents=True)
        library.write_bytes(b"synthetic")
        environment = {"PATH": str(self.root)}
        with patch.object(recovery, "compiler_environment", return_value=environment), \
             patch.object(recovery.shutil, "which", return_value="cmake.exe"), \
             patch.object(recovery.subprocess, "run",
                          return_value=SimpleNamespace(returncode=0)) as run:
            found, elapsed = recovery._build_library(build, self.root)
        self.assertEqual(found, library.resolve())
        self.assertGreaterEqual(elapsed, 0)
        self.assertIs(run.call_args.kwargs["env"], environment)
        self.assertEqual(run.call_args.args[0][0], "cmake.exe")

    def test_oracle_runtime_flag_explicit_mt_and_legacy_default_md(self) -> None:
        environment = {"PATH": str(self.root)}
        completed = SimpleNamespace(returncode=0, stdout="", stderr="")
        with patch.object(semantic_batch, "compiler_environment", return_value=environment), \
             patch.object(semantic_batch.shutil, "which", return_value="clang-cl.exe"), \
             patch.object(semantic_batch.subprocess, "run", return_value=completed) as run:
            semantic_batch.compile_and_run("runtime-mt", b"", b"", [], self.root,
                                           msvc_runtime="MT")
            mt_command = run.call_args_list[0].args[0]
            run.reset_mock()
            semantic_batch.compile_and_run("runtime-md", b"", b"", [], self.root)
            md_command = run.call_args_list[0].args[0]
        self.assertIn("/MT", mt_command)
        self.assertNotIn("/MD", mt_command)
        self.assertIn("/MD", md_command)
        self.assertNotIn("/MT", md_command)

    def test_library_mode_requires_runtime_and_invalidates_old_pass(self) -> None:
        harness = self.root / "oracle.cpp"
        harness.write_text("", encoding="utf-8")
        batch = self.root / "batch.json"
        batch.write_text(json.dumps({"families": [{"name": "synthetic", "manifest":
            str(self.manifest), "harness": str(harness), "sources": []}]}), encoding="utf-8")
        output = self.root / "result"
        output.mkdir()
        receipt = output / "synthetic-result.json"
        receipt.write_text('{"status":"passed"}', encoding="utf-8")
        with patch.object(recovery, "_build_library") as build:
            with self.assertRaisesRegex(ValueError, "--msvc-runtime is required"):
                recovery.run_batch(batch, output, self.ppc, library_build=self.root / "build")
        build.assert_not_called()
        self.assertEqual(json.loads(receipt.read_text())["status"], "failed")

    def test_batch_reuses_one_native_environment_for_build_and_two_oracles(self) -> None:
        harness = self.root / "oracle.cpp"
        harness.write_text("", encoding="utf-8")
        families = []
        for index, entry in enumerate(self.entries):
            manifest = self.root / f"family-{index}.json"
            manifest.write_text(json.dumps({"entries": [entry]}), encoding="utf-8")
            families.append({"name": f"family-{index}", "manifest": str(manifest),
                             "harness": str(harness), "sources": []})
        batch = self.root / "batch.json"
        batch.write_text(json.dumps({"families": families}), encoding="utf-8")
        library = self.root / "LostOdysseyRecompSemantics.lib"
        library.write_bytes(b"synthetic")
        environment = {"PATH": str(self.root)}
        completed = SimpleNamespace(returncode=0, stdout="PASS synthetic", stderr="")
        with patch.object(recovery, "compiler_environment", return_value=environment) as setup, \
             patch.object(recovery, "_build_library", return_value=(library, 0)) as build, \
             patch.object(semantic_batch, "compiler_environment") as repeated_setup, \
             patch.object(semantic_batch.shutil, "which", return_value="clang-cl.exe"), \
             patch.object(semantic_batch.subprocess, "run", return_value=completed) as run:
            result = recovery.run_batch(batch, self.root / "result", self.ppc,
                                        library_build=self.root / "build", msvc_runtime="MT")
        setup.assert_called_once()
        repeated_setup.assert_not_called()
        self.assertIs(build.call_args.kwargs["native_environment"], environment)
        self.assertEqual(run.call_count, 4)
        self.assertTrue(all(call.kwargs["env"] is environment for call in run.call_args_list))
        self.assertEqual(result["status"], "passed")
        self.assertGreaterEqual(result["compiler_setup_seconds"], 0)

    def test_compose_keeps_duplicate_family_oracles_and_shared_pin(self) -> None:
        harness = self.root / "oracle.cpp"
        harness.write_text("", encoding="utf-8")
        paths = []
        for index in range(2):
            batch = self.root / f"batch-{index}.json"
            batch.write_text(json.dumps({"schema": "semantic-recovery-batch-v1",
                "families": [{"name": "synthetic", "manifest": str(self.manifest),
                              "harness": str(harness), "prelude": f"// {index}",
                              "sources": []}]}), encoding="utf-8")
            paths.append(batch)
        composed = recovery.compose_batches(paths)
        self.assertEqual([item["name"] for item in composed["families"]],
                         ["synthetic", "synthetic-2"])
        self.assertEqual([item["prelude"] for item in composed["families"]],
                         ["// 0", "// 1"])
        plan = recovery.check_composed_batch(composed, self.ppc)
        self.assertEqual([item["receipt_name"] for item in plan],
                         ["synthetic-result.json", "synthetic-2-result.json"])
        combined = self.root / "combined.json"
        combined.write_text(json.dumps(composed), encoding="utf-8")
        library = self.root / "LostOdysseyRecompSemantics.lib"
        library.write_bytes(b"synthetic")
        environment = {"PATH": str(self.root)}
        completed = SimpleNamespace(returncode=0, stdout="PASS synthetic", stderr="")
        with patch.object(recovery, "compiler_environment", return_value=environment), \
             patch.object(recovery, "_build_library", return_value=(library, 0)) as build, \
             patch.object(semantic_batch.shutil, "which", return_value="clang-cl.exe"), \
             patch.object(semantic_batch.subprocess, "run", return_value=completed) as run:
            result = recovery.run_batch(combined, self.root / "result", self.ppc,
                                        library_build=self.root / "build",
                                        msvc_runtime="MT", allow_shared_pins=True)
        build.assert_called_once()
        self.assertEqual(run.call_count, 4)
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["source_batches"], [str(path.resolve()) for path in paths])
        self.assertEqual([item["source_batch"] for item in result["families"]],
                         [str(path.resolve()) for path in paths])

    def test_shared_pin_requires_same_source_line_and_body(self) -> None:
        checked = recovery.validate_manifests([self.manifest], self.ppc)[0]
        seen = {}
        recovery._track_pins(checked, seen, allow_shared=True)
        recovery._track_pins(checked, seen, allow_shared=True)
        changed = dict(checked)
        changed["pin_identities"] = dict(checked["pin_identities"])
        changed["pin_identities"]["80000000"] = ("ppc_recomp.0.cpp", 1, "other body")
        with self.assertRaisesRegex(ValueError, "conflicting shared pin: 80000000"):
            recovery._track_pins(changed, seen, allow_shared=True)
        with self.assertRaisesRegex(ValueError, "duplicate batch address: 80000000"):
            recovery._track_pins(checked, seen, allow_shared=False)

    def test_serve_reuses_environment_for_two_requests(self) -> None:
        harness = self.root / "oracle.cpp"
        harness.write_text("", encoding="utf-8")
        batch = self.root / "batch.json"
        batch.write_text(json.dumps({"schema": "semantic-recovery-batch-v1",
            "families": [{"name": "synthetic", "manifest": str(self.manifest),
                          "harness": str(harness), "sources": []}]}), encoding="utf-8")
        library = self.root / "LostOdysseyRecompSemantics.lib"
        library.write_bytes(b"synthetic")
        environment = {"PATH": "SECRET_ENV_VALUE"}
        completed = SimpleNamespace(returncode=0, stdout="PASS synthetic", stderr="")
        outputs = [self.root / f"result-{index}" for index in range(2)]
        requests = [json.dumps({"batches": [str(batch)], "output": str(output)})
                    for output in outputs]
        requests.append(json.dumps({"action": "exit"}))
        captured = io.StringIO()
        with patch.object(recovery, "compiler_environment", return_value=environment) as setup, \
             patch.object(recovery, "_build_library", return_value=(library, 0)) as build, \
             patch.object(semantic_batch, "compiler_environment") as repeated_setup, \
             patch.object(semantic_batch.shutil, "which", return_value="clang-cl.exe"), \
             patch.object(semantic_batch.subprocess, "run", return_value=completed) as run, \
             redirect_stdout(captured):
            recovery.serve_requests(requests, self.ppc, self.root / "build", None, "MT")
        setup.assert_called_once()
        repeated_setup.assert_not_called()
        self.assertEqual(build.call_count, 2)
        self.assertTrue(all(call.kwargs["native_environment"] is environment
                            for call in build.call_args_list))
        self.assertEqual(run.call_count, 4)
        self.assertTrue(all(call.kwargs["env"] is environment for call in run.call_args_list))
        markers = [json.loads(line.removeprefix("SEMANTIC_RECOVERY_DONE "))
                   for line in captured.getvalue().splitlines()
                   if line.startswith("SEMANTIC_RECOVERY_DONE ")]
        self.assertEqual([marker["status"] for marker in markers],
                         ["passed", "passed", "exit"])
        self.assertNotIn("SECRET_ENV_VALUE", captured.getvalue())
        for output in outputs:
            self.assertEqual(json.loads((output / "semantic-recovery-result.json")
                                        .read_text())["status"], "passed")

    def test_serve_keeps_failed_receipt_and_continues(self) -> None:
        bad_batch = self.root / "bad-batch.json"
        bad_batch.write_text('{"schema":"wrong"}', encoding="utf-8")
        harness = self.root / "oracle.cpp"
        harness.write_text("", encoding="utf-8")
        good_batch = self.root / "good-batch.json"
        good_batch.write_text(json.dumps({"schema": "semantic-recovery-batch-v1",
            "families": [{"name": "synthetic", "manifest": str(self.manifest),
                          "harness": str(harness), "sources": []}]}), encoding="utf-8")
        library = self.root / "LostOdysseyRecompSemantics.lib"
        library.write_bytes(b"synthetic")
        outputs = [self.root / "failed-result", self.root / "passed-result"]
        requests = [json.dumps({"batches": [str(batch)], "output": str(output)})
                    for batch, output in zip((bad_batch, good_batch), outputs)]
        captured = io.StringIO()
        with patch.object(recovery, "compiler_environment", return_value={"PATH": str(self.root)}) as setup, \
             patch.object(recovery, "_build_library", return_value=(library, 0)) as build, \
             patch.object(semantic_batch.shutil, "which", return_value="clang-cl.exe"), \
             patch.object(semantic_batch.subprocess, "run",
                          return_value=SimpleNamespace(returncode=0, stdout="PASS", stderr="")), \
             redirect_stdout(captured):
            recovery.serve_requests(requests, self.ppc, self.root / "build", None, "MT")
        setup.assert_called_once()
        build.assert_called_once()
        markers = [json.loads(line.removeprefix("SEMANTIC_RECOVERY_DONE "))
                   for line in captured.getvalue().splitlines()
                   if line.startswith("SEMANTIC_RECOVERY_DONE ")]
        self.assertEqual([marker["status"] for marker in markers], ["failed", "passed"])
        failed = json.loads((outputs[0] / "semantic-recovery-result.json").read_text())
        self.assertEqual(failed["phase"], "compose")
        self.assertIn("unexpected batch schema", failed["error"])
        self.assertEqual(json.loads((outputs[1] / "semantic-recovery-result.json")
                                    .read_text())["status"], "passed")


if __name__ == "__main__":
    unittest.main()
