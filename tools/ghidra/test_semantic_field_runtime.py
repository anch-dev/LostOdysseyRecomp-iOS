"""Compare new native field wrappers with original PPC using existing seed fixtures."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

from generate_semantic_field_wrappers import generate
from semantic_batch import extract_originals
from test_semantic_accessor_runtime import make_originals
import test_semantic_field_bits as bits
import test_semantic_field_arithmetic as arithmetic


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402


def runtime_harness(fixture, entries: list[dict]) -> str:
    # Reuse the existing independent memory/context setup and comparison. Only
    # replace the semantic invocation with the actual strong linked wrapper.
    source = fixture.make_harness(entries).decode("utf-8")
    start = source.index("    Registers registers{recovered.r3.u64")
    last = "    recovered.r11.u64 = registers.r11;"
    end = source.index(last, start) + len(last)
    source = source[:start] + "    RunWrapper(entry.address, case_index, recovered, space.data());" + source[end:]
    source = source[:source.index("int main()")]
    table, cases = (("kFieldBitEntries", "kCasesPerEntry") if fixture is bits else
                    ("kTestEntries", "kCases"))
    source += f"""
int main()
{{
    SparseGuestSpace space;
    // First and last existing seeds exercise direct/table dispatch, high bits,
    // address wrap or cursor aliasing without rerunning the old whole suite.
    for (const auto& entry : {table})
        for (const unsigned seed : {{0u, {cases} - 1}})
            if (!Test(entry, seed, space)) return 1;
    std::printf("PASS %zu field wrappers, direct + table, %s\\n",
        std::size({table}),
        lo::runtime::semantic_fields::Enabled() ? "enabled" : "disabled");
}}
"""
    declarations = ['#include "cpu/semantic_fields.h"', '#include <stdexcept>',
                    'extern PPCFunc* DirectCalls[];',
                    'extern unsigned g_original_calls[];']
    declarations.extend(f'extern "C" PPC_FUNC(__imp__sub_{entry["address"]});' for entry in entries)
    declarations.append(f"""
void RunWrapper(std::uint32_t address, unsigned seed, PPCContext& ctx,
                std::uint8_t* base)
{{
    std::size_t index = 0;
    while (index != {len(entries)} && PPCFuncMappings[index].guest != address) ++index;
    if (index == {len(entries)}) throw std::runtime_error("missing field mapping");
    const unsigned before = g_original_calls[index];
    (seed == 0 ? DirectCalls[index] : PPCFuncMappings[index].host)(ctx, base);
    const unsigned expected = before + (lo::runtime::semantic_fields::Enabled() ? 0u : 1u);
    if (g_original_calls[index] != expected)
        throw std::runtime_error("field wrapper used the wrong fallback path");
}}
""")
    return "\n".join(declarations) + "\n" + source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-field-runtime-tests")
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
    gate = "LO_SEMANTIC_FIELDS_RUNTIME"
    disabled_env = {key: value for key, value in env.items() if key.upper() != gate}
    results = []
    for family, fixture in (("field_bits", bits), ("field_arithmetic", arithmetic)):
        destination = output / family
        destination.mkdir(exist_ok=True)
        manifest = json.loads(fixture.MANIFEST.read_text(encoding="utf-8"))
        entries = manifest["entries"]
        original = extract_originals(entries, args.ppc_root)
        fixture.check_originals(entries, original)
        bodies = [match.group() for match in fixture.BODY.finditer(original)]
        (destination / "originals.cpp").write_bytes(make_originals(entries, bodies))
        (destination / "wrappers.cpp").write_text(generate(manifest, family), encoding="utf-8")
        (destination / "harness.cpp").write_text(runtime_harness(fixture, entries), encoding="utf-8")
        obj = destination / "originals.obj"
        archive = destination / "originals.lib"
        executable = destination / "runtime_test.exe"
        subprocess.run([*common, "/c", str(destination / "originals.cpp"), "/Fo" + str(obj)],
                       cwd=ROOT, env=env, check=True)
        subprocess.run([librarian, "/nologo", "/OUT:" + str(archive), str(obj)],
                       cwd=ROOT, env=env, check=True)
        subprocess.run([*common, str(destination / "wrappers.cpp"),
                        str(destination / "harness.cpp"), str(archive),
                        "/Fe" + str(executable), "/Fo" + str(destination) + "\\"],
                       cwd=ROOT, env=env, check=True)
        for enabled in (False, True):
            run_env = dict(disabled_env, **({gate: "1"} if enabled else {}))
            result = subprocess.run([str(executable)], cwd=ROOT, env=run_env,
                                    check=True, capture_output=True, text=True)
            print(result.stdout.strip())
        results.append(dict(family=family, entries=len(entries), comparisons=len(entries) * 4))
    (output / "field-runtime-result.json").write_text(json.dumps(
        dict(status="passed", batches=results,
             scope="Original PPC context and seeded ordinary memory; actual direct/table strong wrappers and fallback counts in both gate modes."),
        indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
