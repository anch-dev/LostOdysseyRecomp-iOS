"""Compare the actual 58 opt-in getter wrappers with cached PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

from generate_semantic_registered_wrappers import generate
from semantic_batch import ROOT

sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402


def make_fixture(entries: list[dict], candidates: dict[str, dict]) -> str:
    targets: set[str] = set()
    bodies: list[str] = []
    fallback: list[str] = []
    table: list[str] = []
    for index, entry in enumerate(entries):
        address = entry["address"]
        candidate = candidates[address]
        original_name = f"PPC_FUNC_IMPL(__imp__sub_{address})"
        if (candidate["generated_ppc_path"] != entry["source"] or
                candidate["line"] != entry["source_line"] or
                len(candidate["instructions"]) != 19 or
                entry["frame_size"] != 96 or
                not candidate["body"].startswith(original_name) or
                re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\)",
                           candidate["body"]) !=
                [entry["constructor"], entry["registration"]]):
            raise ValueError(f"getter cache/manifest changed: {address}")
        bodies.append(candidate["body"].replace(
            original_name, f"PPC_FUNC_IMPL(reference_sub_{address})", 1))
        fallback.append(
            f'extern "C" PPC_FUNC(__imp__sub_{address}) '
            f'{{ ++g_fallback_calls[{index}]; '
            f'reference_sub_{address}(ctx, base); }}')
        table.append("    {" + ", ".join((
            f"0x{address}u", f"{entry['singleton_address']}u",
            f"0x{entry['constructor']}u", f"0x{entry['registration']}u",
            f"{entry['owner']}ull", f"reference_sub_{address}",
            f"sub_{address}")) + "},")
        targets.update((entry["constructor"], entry["registration"]))
    declarations = ["extern unsigned g_fallback_calls[58];",
                    "void GuestCall(PPCContext&, std::uint8_t*, std::uint32_t, bool);",
                    *(f"PPC_FUNC(sub_{address});" for address in sorted(targets)),
                    *(f"PPC_FUNC(table_{address});" for address in sorted(targets)),
                    *(f"PPC_FUNC(sub_{entry['address']});" for entry in entries)]
    stubs = [line for address in sorted(targets) for line in (
        f"PPC_FUNC(sub_{address}) {{ GuestCall(ctx, base, 0x{address}u, false); }}",
        f"PPC_FUNC(table_{address}) {{ GuestCall(ctx, base, 0x{address}u, true); }}")]
    original = "\n".join([*declarations, *bodies, *fallback, *stubs])
    harness_path = ROOT / "LostOdysseyRecompSemantics/tests/registered_getter_runtime_oracle.cpp"
    harness = harness_path.read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1 or harness.count("/* TARGET_TABLE */") != 1:
        raise ValueError("runtime getter harness markers changed")
    target_table = "\n".join(
        f"    {{0x{address}u, table_{address}}}," for address in sorted(targets))
    harness = harness.replace("/* ENTRY_TABLE */", "\n".join(table))
    harness = harness.replace("/* TARGET_TABLE */", target_table)
    return '#include "ppc_context.h"\n' + original + "\n" + harness


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-registered-runtime-tests")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        parser.error("output must be outside the checkout and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/registered_getter_families.json")
                          .read_text(encoding="utf-8"))
    entries = manifest["entries"]
    addresses = [entry["address"] for entry in entries]
    if manifest["entry_count"] != 58 or len(entries) != 58 or \
            addresses != sorted(set(addresses)):
        raise ValueError("registered getter inventory changed")
    caches = [json.loads((ROOT / "out/function-inventory" / name)
                         .read_text(encoding="utf-8")) for name in (
        "registered-dependency-candidates.json", "registered-next-dependencies.json")]
    candidates = {item["address"]: item for catalog in caches for item in catalog}
    if len(candidates) != sum(len(catalog) for catalog in caches) or \
            not set(addresses) <= candidates.keys():
        raise ValueError("registered getter PPC cache changed")
    fixture = output / "registered-getter-runtime-fixture.cpp"
    wrappers = output / "registered-getter-runtime-wrappers.cpp"
    executable = output / "registered-getter-runtime.exe"
    fixture.write_text(make_fixture(entries, candidates), encoding="utf-8")
    wrapper_source = generate(manifest)
    if wrapper_source.count("PPC_FUNC(sub_") != 58:
        raise ValueError("registered wrapper count changed")
    wrappers.write_text(wrapper_source, encoding="utf-8")
    receipt = output / "registered-getter-runtime-result.json"
    receipt.write_text(json.dumps({"status": "failed"}) + "\n", encoding="utf-8")
    env = compiler_environment()
    search_path = next(value for key, value in env.items() if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    if not compiler:
        raise RuntimeError("clang-cl unavailable")
    includes = [ROOT / "LostOdysseyRecompLib/ppc",
                ROOT / "tools/XenonRecomp/thirdparty/simde",
                ROOT / "LostOdysseyRecompSemantics/include", ROOT / "LostOdysseyRecomp"]
    command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MD",
               "/D_CRT_SECURE_NO_WARNINGS", "-Wno-ignored-attributes", "-msse4.1",
               *("/I" + str(path) for path in includes),
               str(fixture), str(wrappers), "/Fo" + str(output) + "\\",
               "/Fe" + str(executable)]
    log = output / "registered-getter-runtime.log"
    started = time.perf_counter()
    compiled = subprocess.run(command, cwd=ROOT, env=env,
                              capture_output=True, text=True, timeout=300)
    log.write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    print(compiled.stdout + compiled.stderr, end="", flush=True)
    if compiled.returncode:
        raise subprocess.CalledProcessError(compiled.returncode, command)
    result = {"suite": "registered-getter-runtime", "status": "failed",
              "compile_seconds": round(time.perf_counter() - started, 3)}
    gate = "LO_SEMANTIC_REGISTERED_RUNTIME"
    disabled = {key: value for key, value in env.items() if key.upper() != gate}
    summaries = []
    for enabled in (False, True):
        run_env = dict(disabled, **({gate: "1"} if enabled else {}))
        started = time.perf_counter()
        completed = subprocess.run([str(executable)], cwd=ROOT, env=run_env,
                                   capture_output=True, text=True, timeout=300)
        text = completed.stdout + completed.stderr
        with log.open("a", encoding="utf-8") as stream:
            stream.write(text)
        print(text, end="", flush=True)
        if completed.returncode:
            raise subprocess.CalledProcessError(completed.returncode, [str(executable)])
        summaries.append({"gate_enabled": enabled,
                          "execute_seconds": round(time.perf_counter() - started, 3),
                          "output": text.strip()})
    result.update(status="passed", entries=58, scenarios_per_gate=63,
                  summaries=summaries,
                  scope="Cached original getter bodies vs actual strong wrappers: full PPCContext, selected RAM/stack, direct/table calls, guest-target table routing, enabled/disabled fallback counts.")
    receipt.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
