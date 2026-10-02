"""Small contract fixtures for the version-checked Ghidra evidence catalog."""

from __future__ import annotations

import hashlib
import json
import sqlite3
from contextlib import closing
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "ghidra" / "analysis_catalog.py"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class CatalogFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name) / "repo"
        (self.repo / "out" / "fresh").mkdir(parents=True)
        (self.repo / "LostOdysseyRecompLib" / "ppc").mkdir(parents=True)
        (self.repo / "LostOdysseyRecomp" / "debug").mkdir(parents=True)
        (self.repo / "docs" / "notes").mkdir(parents=True)
        self.xex = b"fixture XEX identity"
        (self.repo / "original.xex").write_bytes(self.xex)
        self.db = self.repo / "out" / "catalog.sqlite"
        self.inventory = self.repo / "out" / "inventory.jsonl"
        self.write_inventory()
        (self.repo / "LostOdysseyRecompLib" / "ppc" / "ppc_recomp.0.cpp").write_text(
            "PPC_FUNC_IMPL(__imp__sub_82000000) { }\n"
            "PPC_FUNC_IMPL(__imp__sub_82000008) { }\n"
            "PPC_FUNC_IMPL(__imp__sub_820000A0) { }\n", encoding="utf-8")
        (self.repo / "LostOdysseyRecomp" / "debug" / "hook.cpp").write_text(
            "// PPC_FUNC(sub_82000080) { }\n"
            "/* PPC_FUNC(sub_82000084) { } */\n"
            "PPC_FUNC(sub_82000088);\n"
            "PPC_FUNC(sub_82000040)\n{ }\n", encoding="utf-8")
        (self.repo / "docs" / "notes" / "finding.md").write_text(
            "See 0x82000040, 0x82000090 and 0x820000C0.\n", encoding="utf-8")
        (self.repo / "out" / "old.txt").write_text(
            "// Requested 82000008: sub_82000000\n"
            "// Entry 82000000\n"
            "// Reference 82000044 sub_82000040\n", encoding="utf-8")
        self.annotations = self.repo / "out" / "annotations.json"
        self.annotations.write_text(json.dumps({
            "schema_version": 1, "source_sha256": digest(self.xex),
            "functions": [{"address": "82000040", "label": "DocumentedFunction",
                           "category": "test", "confidence": "documented",
                           "evidence": [{"path": "docs/notes/finding.md", "line": 1,
                                         "note": "separate evidence note"}]}]
        }), encoding="utf-8")

    def write_inventory(self, sha=None, complete=True):
        records = [
            {"kind": "program", "name": "fixture", "executable_path": "original.xex",
             "executable_sha256": sha or digest(self.xex), "image_base": "82000000",
             "language": "PowerPC:BE:32", "compiler_spec": "default", "function_count": 3},
            {"kind": "function", "address": "82000000", "name": "sub_82000000",
             "signature": "void sub_82000000(void)", "body_ranges": [["82000000", "8200001F"]],
             "is_thunk": False, "is_external": False},
            {"kind": "function", "address": "82000040", "name": "sub_82000040",
             "signature": "void sub_82000040(void)", "body_ranges": [["82000040", "8200005F"]],
             "is_thunk": False, "is_external": False},
            {"kind": "function", "address": "82000080", "name": "sub_82000080",
             "signature": "void sub_82000080(void)", "body_ranges": [["82000080", "8200009F"]],
             "is_thunk": False, "is_external": False},
            {"kind": "call", "caller": "82000000", "callee": "82000040",
             "site": "82000004", "reference_type": "UNCONDITIONAL_CALL", "computed": False},
        ]
        if complete:
            records.append({"kind": "complete", "function_count": 3, "call_count": 1})
        self.inventory.write_text("".join(json.dumps(item) + "\n" for item in records), encoding="utf-8")

    def write_fresh(self, warning=False):
        content = (b"void sub_82000000(void) {\n"
                   b"/* WARNING: Control flow encountered bad instruction data */\n"
                   b"/* WARNING: Globals overlap smaller symbols at the same address */\n"
                   b"/* WARNING: Bad instruction - Truncating control flow here */\n"
                   b"halt_baddata();\n}\n" if warning else b"void sub_82000000(void) {}\n")
        (self.repo / "out" / "fresh" / "82000008.c").write_bytes(content)
        records = [
            {"kind": "program", "name": "fixture", "executable_path": "original.xex",
             "executable_sha256": digest(self.xex), "image_base": "82000000",
             "language": "PowerPC:BE:32", "compiler_spec": "default"},
            {"kind": "decomp", "requested": "82000008", "address": "82000000",
             "name": "sub_82000000", "status": "ok", "error": None,
             "filename": "82000008.c", "sha256": digest(content)},
            {"kind": "decomp", "requested": "82000040", "address": "82000040",
             "name": "sub_82000040", "status": "failed", "error": "timeout",
             "filename": None, "sha256": None},
            {"kind": "complete", "request_count": 2, "ok_count": 1,
             "missing_count": 0, "failed_count": 1},
        ]
        (self.repo / "out" / "fresh" / "manifest.jsonl").write_text(
            "".join(json.dumps(item) + "\n" for item in records), encoding="utf-8")

    def run_cli(self, *args, success=True):
        result = subprocess.run([sys.executable, str(SCRIPT), *map(str, args)],
                                text=True, capture_output=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def build(self, fresh=False, success=True):
        args = ["build", "--repo", self.repo, "--inventory", "out/inventory.jsonl",
                "--xex", "original.xex", "--annotations", "out/annotations.json",
                "--output", "out/catalog.sqlite"]
        if fresh:
            args += ["--decomp-dir", "out/fresh"]
        return self.run_cli(*args, success=success)

    def test_sha_mismatch_and_incomplete_inventory_preserve_existing_db(self):
        self.db.write_bytes(b"keep existing catalog")
        self.write_inventory(sha="0" * 64)
        result = self.build(success=False)
        self.assertIn("SHA256 mismatch", result.stderr)
        self.assertEqual(self.db.read_bytes(), b"keep existing catalog")
        self.write_inventory(complete=False)
        result = self.build(success=False)
        self.assertIn("incomplete inventory", result.stderr)
        self.assertEqual(self.db.read_bytes(), b"keep existing catalog")

    def test_interior_legacy_mapping_hook_filter_and_distinct_evidence(self):
        self.write_fresh()
        self.build(fresh=True)
        shown = json.loads(self.run_cli("show", "--db", self.db, "82000008").stdout)
        self.assertEqual(shown["functions"][0]["address"], "82000000")
        self.assertEqual(shown["legacy_mappings"][0]["entry"], "82000000")
        self.assertEqual(shown["decomp_mappings"][0]["status"], "ok")
        self.assertIn("out/fresh/82000008.c", {item["path"] for item in shown["evidence"]})
        self.assertEqual(shown["outgoing_calls"][0]["callee"], "82000040")
        with closing(sqlite3.connect(self.db)) as db:
            hooks = {row[0] for row in db.execute(
                "SELECT address FROM evidence WHERE kind='source_hook'")}
            self.assertEqual(hooks, {"82000040"})
            self.assertEqual(db.execute("SELECT count(*) FROM decomp WHERE status='failed'").fetchone()[0], 1)
        annotated = json.loads(self.run_cli("query", "--db", self.db, "separate evidence note").stdout)
        self.assertEqual(annotated[0]["annotations"][0]["label"], "DocumentedFunction")
        report = self.run_cli("report", "--db", self.db).stdout
        self.assertIn("Fresh exports (pseudocode produced) | 1", report)
        self.assertIn("Generated PPC entries in local source | 3", report)
        self.assertIn("Generated PPC entries matching Ghidra inventory addresses | 1", report)
        self.assertIn("Generated PPC addresses without exact Ghidra entry | 2", report)
        self.assertIn("Of those, inside a Ghidra function body | 1", report)
        self.assertIn("Of those, outside Ghidra function bodies | 1", report)
        self.assertIn("Fresh missing / failed requests | 0 / 1", report)
        self.assertIn("Document mentions inside Ghidra bodies, otherwise unlinked | 1", report)
        self.assertIn("Document-only addresses outside Ghidra bodies | 1", report)
        self.assertIn("82000040", report)  # source hook has no successful fresh decomp
        self.run_cli("report", "--db", self.db, "--output", "report.md")
        self.assertTrue((self.repo / "out" / "report.md").is_file())

    def test_fresh_file_hash_mismatch_does_not_replace_catalog(self):
        self.write_fresh()
        self.build(fresh=True)
        before = self.db.read_bytes()
        (self.repo / "out" / "fresh" / "82000008.c").write_text("changed", encoding="utf-8")
        result = self.build(fresh=True, success=False)
        self.assertIn("decomp file SHA256 mismatch", result.stderr)
        self.assertEqual(self.db.read_bytes(), before)

    def test_multiple_manifests_and_invalid_requested_mapping(self):
        self.write_fresh()
        second = self.repo / "out" / "second"
        second.mkdir()
        (second / "manifest.jsonl").write_bytes(
            (self.repo / "out" / "fresh" / "manifest.jsonl").read_bytes())
        (second / "82000008.c").write_bytes(
            (self.repo / "out" / "fresh" / "82000008.c").read_bytes())
        self.run_cli("build", "--repo", self.repo, "--inventory", "out/inventory.jsonl",
                     "--xex", "original.xex", "--output", "out/catalog.sqlite",
                     "--decomp-dir", "out/fresh", "--decomp-dir", "out/second")
        with closing(sqlite3.connect(self.db)) as db:
            self.assertEqual(db.execute("SELECT count(*) FROM decomp").fetchone()[0], 4)
        before = self.db.read_bytes()
        manifest = second / "manifest.jsonl"
        records = [json.loads(line) for line in manifest.read_text(encoding="utf-8").splitlines()]
        records[1]["address"] = "82000040"  # requested address remains inside another function
        manifest.write_text("".join(json.dumps(item) + "\n" for item in records), encoding="utf-8")
        result = self.run_cli("build", "--repo", self.repo, "--inventory", "out/inventory.jsonl",
                              "--xex", "original.xex", "--output", "out/catalog.sqlite",
                              "--decomp-dir", "out/second", success=False)
        self.assertIn("does not match an inventory function/body range", result.stderr)
        self.assertEqual(self.db.read_bytes(), before)

    def test_warning_lines_distinguish_bad_instruction_from_overlap(self):
        self.write_fresh(warning=True)
        other = self.repo / "out" / "overlap"
        other.mkdir()
        other_c = b"/* WARNING: Globals overlap smaller symbols at the same address */\nvoid f() {}\n"
        (other / "82000040.c").write_bytes(other_c)
        program = json.loads((self.repo / "out" / "fresh" / "manifest.jsonl").read_text(
            encoding="utf-8").splitlines()[0])
        records = [
            program,
            {"kind": "decomp", "requested": "82000040", "address": "82000040",
             "name": "sub_82000040", "status": "ok", "error": None,
             "filename": "82000040.c", "sha256": digest(other_c)},
            {"kind": "complete", "request_count": 1, "ok_count": 1,
             "missing_count": 0, "failed_count": 0},
        ]
        (other / "manifest.jsonl").write_text("".join(json.dumps(item) + "\n" for item in records),
                                              encoding="utf-8")
        self.run_cli("build", "--repo", self.repo, "--inventory", "out/inventory.jsonl",
                     "--xex", "original.xex", "--output", "out/catalog.sqlite",
                     "--decomp-dir", "out/fresh", "--decomp-dir", "out/overlap")
        shown = json.loads(self.run_cli("show", "--db", self.db, "82000000").stdout)
        markers = [(item["line"], item["detail"]) for item in shown["evidence"]
                   if item["kind"] == "fresh_decomp_warning" and item["address"] == "82000000"]
        self.assertIn((2, "/* WARNING: Control flow encountered bad instruction data */"), markers)
        self.assertIn((4, "/* WARNING: Bad instruction - Truncating control flow here */"), markers)
        self.assertIn((5, "halt_baddata();"), markers)
        found = json.loads(self.run_cli("query", "--db", self.db, "halt_baddata").stdout)
        self.assertEqual(found[0]["requested_address"], "82000000")
        report = self.run_cli("report", "--db", self.db).stdout
        self.assertIn("Fresh exports (pseudocode produced) | 2", report)
        self.assertIn("Fresh exports with bad instruction / truncated control flow markers | 1", report)
        self.assertIn("Fresh exports with other warnings only | 1", report)
        self.assertIn("Fresh exports with no scanned warning markers | 0", report)


if __name__ == "__main__":
    unittest.main()
