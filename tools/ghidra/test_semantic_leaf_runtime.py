"""Link all leaf wrappers against weak PPC functions and test both call paths."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402
from generate_semantic_leaf_wrappers import generate, generate_integer  # noqa: E402

MANIFEST = ROOT / "LostOdysseyRecompSemantics/leaf_families.json"
INTEGER_MANIFEST = ROOT / "LostOdysseyRecompSemantics/integer_leaf_families.json"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-leaf-runtime-test"


def make_originals(entries: list[dict]) -> str:
    # Declare before the table, then define the weak originals and direct callers.
    prefix = ['#include "ppc_context.h"',
              "unsigned g_original_calls[" + str(len(entries)) + "]{};"]
    for entry in entries:
        address = entry["address"]
        prefix.extend([f"PPC_FUNC(sub_{address});", f"PPC_FUNC(Direct_{address});"])
    prefix.append("PPCFunc* DirectCalls[] = {")
    prefix.extend(f"    &Direct_{entry['address']}," for entry in entries)
    prefix.extend(["};", "PPCFuncMapping PPCFuncMappings[] = {"])
    prefix.extend(f"    {{0x{entry['address']}, &sub_{entry['address']}}}," for entry in entries)
    prefix.extend(["    {0, nullptr},", "};"])
    for index, entry in enumerate(entries):
        address = entry["address"]
        prefix.extend([
            f'extern "C" PPC_FUNC(__imp__sub_{address})',
            "{",
            f"    ++g_original_calls[{index}];",
            "    ctx.r3.u64 = 0xFEDCBA9876543210ull;",
            "}",
            f"PPC_WEAK_FUNC(sub_{address})",
            "{",
            f"    __imp__sub_{address}(ctx, base);",
            "}",
            f"PPC_FUNC(Direct_{address})",
            "{",
            f"    sub_{address}(ctx, base);",
            "}",
        ])
    return "\n".join(prefix) + "\n"


def make_harness(entries: list[dict], templates: dict) -> str:
    modes = [templates[entry["template"]]["semantic"] for entry in entries]
    lines = [
        '#include "ppc_context.h"',
        "#include <cstdio>",
        "#include <cstring>",
        "extern unsigned g_original_calls[];",
        "extern PPCFunc* DirectCalls[];",
        "static constexpr bool kReturnOne[] = {",
        *(f"    {'true' if mode == 'ReturnOne' else 'false'}," for mode in modes),
        "};",
        "int main(int argc, char** argv)",
        "{",
        '    const bool enabled = argc == 2 && std::strcmp(argv[1], "enabled") == 0;',
        "    alignas(32) uint8_t base[32]{};",
        "    for (unsigned path = 0; path != 2; ++path)",
        "    {",
        f"        for (unsigned i = 0; i != {len(entries)}; ++i)",
        "        {",
        "            PPCContext ctx{};",
        "            const uint64_t original_r3 = 0x12345678ABCDEF01ull + i;",
        "            ctx.r3.u64 = original_r3;",
        "            ctx.r8.u64 = 0xFEDCBA9876543210ull + i;",
        "            ctx.r4.u64 = 0x98765432ABCDEF01ull + i;",
        "            ctx.lr = 0xAABBCCDDEEFF0011ull + i;",
        "            unsigned char before[sizeof ctx];",
        "            std::memcpy(before, &ctx, sizeof ctx);",
        "            const unsigned calls_before = g_original_calls[i];",
        "            (path == 0 ? DirectCalls[i] : PPCFuncMappings[i].host)(ctx, base);",
        "            const uint64_t expected_r3 = !enabled ? 0xFEDCBA9876543210ull",
        "                : kReturnOne[i] ? 1ull : original_r3;",
        "            if (ctx.r3.u64 != expected_r3 ||",
        "                g_original_calls[i] != calls_before + (enabled ? 0u : 1u))",
        "            {",
        '                std::fprintf(stderr, "dispatch failed: path=%u entry=%u\\n", path, i);',
        "                return 1;",
        "            }",
        "            ctx.r3.u64 = original_r3;",
        "            if (std::memcmp(&ctx, before, sizeof ctx) != 0)",
        "            {",
        '                std::fprintf(stderr, "context changed: path=%u entry=%u\\n", path, i);',
        "                return 2;",
        "            }",
        "        }",
        "    }",
        f'    std::printf("%u wrappers: direct + mapping, %s\\n", {len(entries)}, enabled ? "semantic" : "original");',
        "}",
    ]
    return "\n".join(lines) + "\n"


def make_integer_harness(entries: list[dict]) -> str:
    lines = [
        '#include "ppc_context.h"',
        '#include "lo_semantics/integer_leaf.h"',
        "#include <cstdio>",
        "#include <cstring>",
        "extern unsigned g_original_calls[];",
        "extern PPCFunc* DirectCalls[];",
        "static constexpr uint32_t kAddresses[] = {",
        *(f"    0x{entry['address']}u," for entry in entries),
        "};",
        "int main(int argc, char** argv)",
        "{",
        '    const bool enabled = argc == 2 && std::strcmp(argv[1], "enabled") == 0;',
        "    alignas(32) uint8_t base[32]{};",
        "    for (unsigned path = 0; path != 2; ++path)",
        "    {",
        f"        for (unsigned i = 0; i != {len(entries)}; ++i)",
        "        {",
        "            PPCContext actual{};",
        "            actual.r3.u64 = 0x12345678ABCDEF01ull + i;",
        "            actual.r4.u64 = 0x98765432ABCDEF01ull + i;",
        "            actual.r5.u64 = 0x7ABCDEFF12345678ull + i;",
        "            actual.r6.u64 = 0x76543210ABCDEF89ull + i;",
        "            actual.r7.u64 = 0xFEDCBA9876543210ull + i;",
        "            actual.r8.u64 = 0xBA9876543210FEDCull + i;",
        "            actual.r10.u64 = 0x3344556677889900ull + i;",
        "            actual.r11.u64 = 0x1122334455667788ull + i;",
        "            actual.lr = 0xAABBCCDDEEFF0011ull + i;",
        "            PPCContext expected;",
        "            std::memcpy(&expected, &actual, sizeof actual);",
        "            if (enabled)",
        "            {",
        "                lo::semantic::integer_leaf::Registers registers{",
        "                    .r3 = actual.r3.u64, .r4 = actual.r4.u64, .r5 = actual.r5.u64,",
        "                    .r6 = actual.r6.u64, .r7 = actual.r7.u64, .r10 = actual.r10.u64,",
        "                    .r11 = actual.r11.u64,",
        "                };",
        "                if (!lo::semantic::integer_leaf::Apply(kAddresses[i], registers)) return 3;",
        "                expected.r3.u64 = registers.r3;",
        "                expected.r10.u64 = registers.r10;",
        "                expected.r11.u64 = registers.r11;",
        "            }",
        "            else expected.r3.u64 = 0xFEDCBA9876543210ull;",
        "            const unsigned calls_before = g_original_calls[i];",
        "            if (PPCFuncMappings[i].guest != kAddresses[i]) return 4;",
        "            (path == 0 ? DirectCalls[i] : PPCFuncMappings[i].host)(actual, base);",
        "            if (g_original_calls[i] != calls_before + (enabled ? 0u : 1u) ||",
        "                std::memcmp(&actual, &expected, sizeof actual) != 0)",
        "            {",
        '                std::fprintf(stderr, "integer dispatch failed: path=%u entry=%u\\n", path, i);',
        "                return 1;",
        "            }",
        "        }",
        "    }",
        f'    std::printf("{len(entries)} integer wrappers: direct + mapping, %s\\n", enabled ? "semantic" : "original");',
        "}",
    ]
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--integer", action="store_true", help="test the integer-leaf wrappers only")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        parser.error("output must be outside the checkout and ownCloud")
    output.mkdir(parents=True, exist_ok=True)

    manifest = json.loads((INTEGER_MANIFEST if args.integer else MANIFEST).read_text(encoding="utf-8"))
    wrappers = generate_integer(manifest) if args.integer else generate(manifest)
    entries = manifest["entries"]
    (output / "wrappers.cpp").write_text(wrappers, encoding="utf-8")
    (output / "originals.cpp").write_text(make_originals(entries), encoding="utf-8")
    harness = make_integer_harness(entries) if args.integer else make_harness(entries, manifest["body_templates"])
    (output / "harness.cpp").write_text(harness, encoding="utf-8")

    env = compiler_environment()
    path = next(value for key, value in env.items() if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=path)
    librarian = shutil.which("lib.exe", path=path)
    if not compiler or not librarian:
        raise RuntimeError("clang-cl and lib.exe are required")
    includes = [ROOT / "LostOdysseyRecompLib/ppc",
                ROOT / "tools/XenonRecomp/thirdparty/simde",
                ROOT / "LostOdysseyRecompSemantics/include", ROOT / "LostOdysseyRecomp"]
    common = [compiler, "/nologo", "/std:c++20", "/O2", "/MD",
              "-Wno-ignored-attributes", "-msse4.1",
              *("/I" + str(include) for include in includes)]

    def run(command: list[str], *, environment: dict = env) -> None:
        subprocess.run(command, cwd=ROOT, env=environment, check=True)

    originals_obj = output / "originals.obj"
    run([*common, "/c", str(output / "originals.cpp"), "/Fo" + str(originals_obj)])
    archive = output / "originals.lib"
    run([librarian, "/nologo", "/OUT:" + str(archive), str(originals_obj)])
    executable = output / "semantic_leaf_runtime_test.exe"
    semantic_source = ("integer_leaf.cpp" if args.integer else "leaf_family.cpp")
    run([*common, str(output / "wrappers.cpp"), str(output / "harness.cpp"),
         str(ROOT / "LostOdysseyRecompSemantics/src" / semantic_source), str(archive),
         "/Fe" + str(executable), "/Fo" + str(output) + "\\"])
    gate = "LO_SEMANTIC_INTEGER_RUNTIME" if args.integer else "LO_SEMANTIC_LEAF_RUNTIME"
    disabled_env = {key: value for key, value in env.items() if key.upper() != gate}
    run([str(executable), "disabled"], environment=disabled_env)
    enabled_env = dict(disabled_env, **{gate: "1"})
    run([str(executable), "enabled"], environment=enabled_env)


if __name__ == "__main__":
    main()
