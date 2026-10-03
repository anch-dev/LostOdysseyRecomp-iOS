"""Compare representative opt-in instance wrappers with cached original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from generate_instance_vtable_family import raw_bodies
from generate_semantic_instance_wrappers import (
    ABI_PATH, ROOT, checked_abi, family_entries, generate, preflight,
    refreshed_abi,
)

sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402


SELECTED = (
    "82405B68",  # single vtable
    "8240A620",  # ordered field / vtable / field
    "82570038",  # two ordered fields
    "825F1018",  # property defaults
    "824D5D80",  # marker byte write
    "8240AB00",  # scalar three zeroes
    "8240AC40",  # scalar four zeroes and eight
    "824D5C80",  # scalar clear before vtable
    "82631AD0",  # UI ordered defaults
)


def fixture(addresses: list[str], bodies: dict[str, list[str]]) -> str:
    declarations = []
    originals = []
    fallbacks = []
    table = []
    for index, address in enumerate(addresses):
        declarations.append(f"PPC_FUNC(sub_{address});")
        original = "\n".join(bodies[address])
        originals.append(original.replace(
            f"PPC_FUNC_IMPL(__imp__sub_{address})",
            f"PPC_FUNC_IMPL(reference_sub_{address})", 1))
        fallbacks.append(
            f'extern "C" PPC_FUNC(__imp__sub_{address}) '
            f'{{ ++g_fallback_calls[{index}]; '
            f'reference_sub_{address}(ctx, base); }}')
        table.append(f"    {{0x{address}u, reference_sub_{address}, "
                     f"sub_{address}, &g_fallback_calls[{index}]}},")
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/instance_runtime_oracle.cpp")
    source = harness.read_text(encoding="utf-8")
    if source.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("instance runtime oracle table marker changed")
    source = source.replace("/* ENTRY_TABLE */", "\n".join(table))
    prefix = ["#include \"ppc_context.h\"", "#include <cstdint>",
              *declarations, f"unsigned g_fallback_calls[{len(addresses)}]{{}};",
              *originals, *fallbacks]
    return "\n".join(prefix) + "\n" + source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-runtime-tests")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        parser.error("output must be outside the checkout and ownCloud")
    output.mkdir(parents=True, exist_ok=True)

    entries = family_entries()
    rows = checked_abi(entries, ABI_PATH)
    if refreshed_abi(entries)["entries"] != rows:
        raise ValueError("strict instance ABI differs from cached PPC")
    preflight(set(entries))
    selected = sorted(set(SELECTED))
    if len(selected) != 9 or not set(selected) <= entries.keys():
        raise ValueError("representative instance shapes changed")
    bodies = raw_bodies(ROOT / "out/function-inventory/registered-instance-originals.cpp.gz")
    selected_rows = [row for row in rows if row["address"] in selected]
    if len(selected_rows) != 9:
        raise ValueError("runtime ABI missing representative shape")
    fixture_path = output / "instance-runtime-fixture.cpp"
    wrapper_path = output / "instance-runtime-wrappers.cpp"
    fixture_path.write_text(fixture(selected, bodies), encoding="utf-8")
    wrapper_path.write_text(generate(selected_rows), encoding="utf-8")
    receipt = output / "instance-runtime-result.json"
    receipt.write_text(json.dumps({"status": "failed"}) + "\n", encoding="utf-8")

    environment = compiler_environment()
    search_path = next(value for key, value in environment.items()
                       if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    if not compiler:
        raise RuntimeError("clang-cl unavailable")
    includes = [ROOT / "LostOdysseyRecompLib/ppc",
                ROOT / "tools/XenonRecomp/thirdparty/simde",
                ROOT / "LostOdysseyRecompSemantics/include",
                ROOT / "LostOdysseyRecomp"]
    sources = [ROOT / "LostOdysseyRecompSemantics/src" / filename for filename in (
        "instance_vtable_family.cpp", "instance_field_initializer_family.cpp",
        "instance_property_initializer_family.cpp",
        "instance_marker_initializer_family.cpp",
        "instance_scalar_initializer_family.cpp", "instance_ui_initializer_family.cpp")]
    executable = output / "instance-runtime.exe"
    command = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MD",
               "-Wno-ignored-attributes", "-msse4.1",
               *("/I" + str(path) for path in includes),
               str(fixture_path), str(wrapper_path), *(str(path) for path in sources),
               "/Fo" + str(output) + "\\", "/Fe" + str(executable)]
    log = output / "instance-runtime.log"
    start = time.perf_counter()
    compiled = subprocess.run(command, cwd=ROOT, env=environment,
                              capture_output=True, text=True, timeout=300)
    log.write_text(compiled.stdout + compiled.stderr, encoding="utf-8")
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr, end="", flush=True)
        raise subprocess.CalledProcessError(compiled.returncode, command)
    summaries = []
    for enabled in (False, True):
        run_env = {key: value for key, value in environment.items()
                   if key.upper() != "LO_SEMANTIC_INSTANCE_RUNTIME"}
        if enabled:
            run_env["LO_SEMANTIC_INSTANCE_RUNTIME"] = "1"
        completed = subprocess.run([str(executable)], cwd=ROOT, env=run_env,
                                   capture_output=True, text=True, timeout=120)
        with log.open("a", encoding="utf-8") as stream:
            stream.write(completed.stdout + completed.stderr)
        print(completed.stdout + completed.stderr, end="", flush=True)
        if completed.returncode:
            raise subprocess.CalledProcessError(completed.returncode, [str(executable)])
        summaries.append({"gate_enabled": enabled,
                          "output": (completed.stdout + completed.stderr).strip()})
    receipt.write_text(json.dumps({"suite": "instance-runtime", "status": "passed",
                                   "mapped_entries": 655, "representative_shapes": 9,
                                   "comparisons_per_gate": 11,
                                   "compile_seconds": round(time.perf_counter() - start, 3),
                                   "summaries": summaries}, indent=2) + "\n",
                       encoding="utf-8")


if __name__ == "__main__":
    main()
