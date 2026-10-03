"""Compare native memory wrappers with PPC through direct and mapping calls."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

from generate_semantic_memory_wrappers import generate
from semantic_batch import extract_originals
from test_semantic_accessor_runtime import make_originals
import test_semantic_read_only_fields as reads
import test_semantic_memory_writes as writes
import test_semantic_single_write_fields as singles


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402


def runtime_harness(fixture, entries: list[dict]) -> str:
    # Reuse the independent input seeds and full context/page comparison.
    # Only the recovered call is replaced by the actual production wrapper.
    source = fixture.make_harness(entries).decode("utf-8")
    start = source.index("    Registers ", source.index("bool Test("))
    end = source.index("    if (std::memcmp(&original", start)
    scenario = "index" if fixture is reads else "case_index"
    source = (source[:start] +
              f"    RunWrapper(entry.address, {scenario}, recovered, space.data());\n" +
              source[end:])
    source = source[:source.index("int main()")]
    source += """
int main()
{
    SparseGuestSpace space;
    for (const auto& entry : kTestEntries)
        for (const unsigned scenario : {0u, kCases - 1})
            if (!Test(entry, scenario, space)) return 1;
    std::printf("PASS %zu memory wrappers, direct + table, %s\\n",
        std::size(kTestEntries),
        lo::runtime::semantic_memory::Enabled() ? "enabled" : "disabled");
}
"""
    declarations = ['#include "cpu/semantic_memory.h"', '#include <stdexcept>',
                    'extern PPCFunc* DirectCalls[];', 'extern unsigned g_original_calls[];']
    declarations.extend(f'extern "C" PPC_FUNC(__imp__sub_{entry["address"]});' for entry in entries)
    declarations.append(f"""
void RunWrapper(std::uint32_t address, unsigned scenario, PPCContext& ctx,
                std::uint8_t* base)
{{
    std::size_t index = 0;
    while (index != {len(entries)} && PPCFuncMappings[index].guest != address) ++index;
    if (index == {len(entries)}) throw std::runtime_error("missing memory mapping");
    const unsigned before = g_original_calls[index];
    (scenario == 0 ? DirectCalls[index] : PPCFuncMappings[index].host)(ctx, base);
    const unsigned expected = before + (lo::runtime::semantic_memory::Enabled() ? 0u : 1u);
    if (g_original_calls[index] != expected)
        throw std::runtime_error("memory wrapper used the wrong fallback path");
}}
""")
    return "\n".join(declarations) + "\n" + source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-memory-runtime-tests")
    parser.add_argument("--family", choices=("read_only_fields", "memory_writes", "single_write_fields"),
                        help="limit a repair rerun to one changed family")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.is_relative_to(Path.home() / "ownCloud"):
        parser.error("output must be outside the checkout and ownCloud")
    output.mkdir(parents=True, exist_ok=True)
    env = compiler_environment()
    search_path = next(value for key, value in env.items() if key.upper() == "PATH")
    compiler = shutil.which("clang-cl", path=search_path)
    librarian = shutil.which("lib.exe", path=search_path)
    if not compiler or not librarian:
        raise RuntimeError("clang-cl and lib.exe are required")
    includes = [ROOT / "LostOdysseyRecompLib/ppc",
                ROOT / "tools/XenonRecomp/thirdparty/simde",
                ROOT / "LostOdysseyRecompSemantics/include", ROOT / "LostOdysseyRecomp"]
    common = [compiler, "/nologo", "/std:c++20", "/EHsc", "/O2", "/MD",
              "/D_CRT_SECURE_NO_WARNINGS", "-Wno-ignored-attributes", "-msse4.1",
              *("/I" + str(include) for include in includes)]
    gate = "LO_SEMANTIC_MEMORY_RUNTIME"
    disabled_env = {key: value for key, value in env.items() if key.upper() != gate}
    results = []
    for family, fixture in (("read_only_fields", reads), ("memory_writes", writes),
                            ("single_write_fields", singles)):
        if args.family and family != args.family:
            continue
        destination = output / family
        destination.mkdir(exist_ok=True)
        manifest = json.loads(fixture.MANIFEST.read_text(encoding="utf-8"))
        entries = manifest["entries"]
        originals = extract_originals(entries, args.ppc_root)
        fixture.check_originals(entries, originals)
        bodies = [match.group() for match in fixture.BODY.finditer(originals)]
        (destination / "originals.cpp").write_bytes(make_originals(entries, bodies))
        (destination / "wrappers.cpp").write_text(generate(manifest, family), encoding="utf-8")
        (destination / "harness.cpp").write_text(runtime_harness(fixture, entries), encoding="utf-8")
        obj, archive = destination / "originals.obj", destination / "originals.lib"
        executable = destination / "runtime_test.exe"
        subprocess.run([*common, "/c", str(destination / "originals.cpp"), "/Fo" + str(obj)],
                       cwd=ROOT, env=env, check=True)
        subprocess.run([librarian, "/nologo", "/OUT:" + str(archive), str(obj)],
                       cwd=ROOT, env=env, check=True)
        subprocess.run([*common, str(destination / "wrappers.cpp"),
                        str(destination / "harness.cpp"), str(fixture.SOURCE), str(archive),
                        "/Fe" + str(executable), "/Fo" + str(destination) + "\\"],
                       cwd=ROOT, env=env, check=True)
        for enabled in (False, True):
            run_env = dict(disabled_env, **({gate: "1"} if enabled else {}))
            result = subprocess.run([str(executable)], cwd=ROOT, env=run_env,
                                    check=True, capture_output=True, text=True)
            print(result.stdout.strip())
        row = dict(family=family, entries=len(entries), comparisons=len(entries) * 4)
        results.append(row)
        (destination / "result.json").write_text(json.dumps(
            dict(status="passed", **row), indent=2) + "\n", encoding="utf-8")
    name = (args.family + "-runtime-result.json") if args.family else "memory-runtime-result.json"
    (output / name).write_text(json.dumps(
        dict(status="passed", batches=results,
             scope="Original PPC full context and seeded ordinary memory; actual direct/table strong wrappers and fallback counts in both gate modes."),
        indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
