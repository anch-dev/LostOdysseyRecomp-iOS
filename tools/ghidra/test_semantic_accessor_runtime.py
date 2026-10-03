"""Link every accessor wrapper with exact PPC bodies and compare both call paths."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

from generate_semantic_leaf_wrappers import generate_accessor
from semantic_batch import extract_originals
from test_semantic_accessor_family import BODY, check_originals


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402

MANIFEST = ROOT / "LostOdysseyRecompSemantics/accessor_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/accessor_runtime_oracle.cpp"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-accessor-runtime-tests"
WIDTHS = {"Byte": ("b", 8), "Halfword": ("h", 16), "Word": ("w", 32)}


def checked_originals(entries: list[dict], ppc_root: Path) -> list[bytes]:
    detailed = []
    for entry in entries:
        operation, bits = WIDTHS[entry["width"]]
        kind = entry["kind"]
        if kind not in ("getter", "setter"):
            raise ValueError(f"unknown accessor kind: {kind}")
        base = entry["base_register"]
        value = 3 if kind == "getter" else entry["value_register"]
        displacement = entry["displacement"]
        if kind == "getter":
            instruction = f"l{operation}z r3,{displacement}(r{base})"
            statement = f"ctx.r3.u64 = PPC_LOAD_U{bits}(ctx.r{base}.u32 + {displacement});"
        else:
            instruction = f"st{operation} r{value},{displacement}(r{base})"
            statement = (f"PPC_STORE_U{bits}(ctx.r{base}.u32 + {displacement}, "
                         f"ctx.r{value}.u{bits});")
        detailed.append({**entry, "instruction_sequence": [instruction, "blr"],
                         "effect_statement": statement})
    originals = extract_originals(detailed, ppc_root)
    check_originals(detailed, originals)
    bodies = [match.group() for match in BODY.finditer(originals)]
    if len(bodies) != len(entries):
        raise ValueError("original PPC body count differs from manifest")
    return bodies


def make_originals(entries: list[dict], bodies: list[bytes]) -> bytes:
    lines = ['#include "ppc_context.h"',
             f"unsigned g_original_calls[{len(entries)}]{{}};"]
    for entry in entries:
        address = entry["address"]
        lines.extend((f"PPC_FUNC(sub_{address});", f"PPC_FUNC(Direct_{address});"))
    lines.extend(("PPCFunc* DirectCalls[] = {",
                  *(f"    &Direct_{entry['address']}," for entry in entries),
                  "};", "PPCFuncMapping PPCFuncMappings[] = {",
                  *(f"    {{0x{entry['address']}, &sub_{entry['address']}}},"
                    for entry in entries),
                  "    {0, nullptr},", "};"))
    source = "\n".join(lines).encode() + b"\n"
    for index, (entry, body) in enumerate(zip(entries, bodies, strict=True)):
        address = entry["address"]
        # Keep the extracted body byte-for-byte. Rename its symbol only at
        # preprocessing so the original fallback can be counted separately.
        source += f"#define __imp__sub_{address} __actual__sub_{address}\n".encode()
        source += body + b"\n"
        source += (f"#undef __imp__sub_{address}\n"
                   f'extern "C" PPC_FUNC(__imp__sub_{address})\n'
                   "{\n"
                   f"    ++g_original_calls[{index}];\n"
                   f"    __actual__sub_{address}(ctx, base);\n"
                   "}\n"
                   f"PPC_WEAK_FUNC(sub_{address})\n"
                   "{\n"
                   f"    __imp__sub_{address}(ctx, base);\n"
                   "}\n"
                   f"PPC_FUNC(Direct_{address})\n"
                   "{\n"
                   f"    sub_{address}(ctx, base);\n"
                   "}\n").encode()
    return source


def make_harness(entries: list[dict]) -> str:
    lines = ['#include "ppc_context.h"',
             "extern PPCFunc* DirectCalls[];", "extern unsigned g_original_calls[];"]
    lines.extend(f'extern "C" PPC_FUNC(__imp__sub_{entry["address"]});'
                 for entry in entries)
    lines.extend(("struct AccessorEntry {",
                  "    std::uint32_t address;",
                  "    PPCFunc* original;",
                  "    bool getter;",
                  "    std::int32_t displacement;",
                  "    unsigned base_register;",
                  "    unsigned value_register;",
                  "};", "static const AccessorEntry kAccessorEntries[] = {"))
    lines.extend(
        f"    {{0x{entry['address']}u, &__imp__sub_{entry['address']}, "
        f"{'true' if entry['kind'] == 'getter' else 'false'}, "
        f"{entry['displacement']}, {entry['base_register']}, "
        f"{entry['value_register'] or 0}}},"
        for entry in entries)
    lines.append("};")
    return "\n".join(lines) + "\n" + ORACLE.read_text(encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path,
                        default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        parser.error("output must be outside the checkout and ownCloud")
    output.mkdir(parents=True, exist_ok=True)

    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    entries = manifest["entries"]
    if len(entries) != 304 or len({entry["address"] for entry in entries}) != 304:
        raise ValueError("expected 304 unique accessors")
    bodies = checked_originals(entries, args.ppc_root)
    (output / "originals.cpp").write_bytes(make_originals(entries, bodies))
    (output / "wrappers.cpp").write_text(generate_accessor(manifest), encoding="utf-8")
    (output / "harness.cpp").write_text(make_harness(entries), encoding="utf-8")

    env = compiler_environment()
    search_path = next(value for key, value in env.items() if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    librarian = shutil.which("lib.exe", path=search_path)
    if not compiler or not librarian:
        raise RuntimeError("clang-cl and lib.exe are required")
    includes = [ROOT / "LostOdysseyRecompLib/ppc",
                ROOT / "tools/XenonRecomp/thirdparty/simde",
                ROOT / "LostOdysseyRecompSemantics/include",
                ROOT / "LostOdysseyRecomp"]
    common = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MD",
              "-Wno-ignored-attributes", "-msse4.1",
              *("/I" + str(include) for include in includes)]

    def run(command: list[str], *, environment: dict = env) -> None:
        subprocess.run(command, cwd=ROOT, env=environment, check=True)

    originals_obj = output / "originals.obj"
    run([*common, "/c", str(output / "originals.cpp"), "/Fo" + str(originals_obj)])
    archive = output / "originals.lib"
    run([librarian, "/nologo", "/OUT:" + str(archive), str(originals_obj)])
    executable = output / "semantic_accessor_runtime_test.exe"
    run([*common, str(output / "wrappers.cpp"), str(output / "harness.cpp"),
         str(archive), "/Fe" + str(executable), "/Fo" + str(output) + "\\"])
    gate = "LO_SEMANTIC_ACCESSOR_RUNTIME"
    disabled_env = {key: value for key, value in env.items() if key.upper() != gate}
    run([str(executable), "disabled"], environment=disabled_env)
    run([str(executable), "enabled"], environment=dict(disabled_env, **{gate: "1"}))


if __name__ == "__main__":
    main()
