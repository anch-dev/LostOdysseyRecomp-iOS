"""Compare native object wrappers with original PPC through both runtime call paths."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from generate_semantic_object_wrappers import generate
from semantic_batch import extract_originals
from test_semantic_accessor_runtime import make_originals
import test_semantic_pointer_fields as pointers
import test_semantic_global_assignments as assignments
import test_semantic_field_operations as operations


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/tests"))
from run import compiler_environment  # noqa: E402

BODY = re.compile(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{\r?\n.*?\r?\n\}", re.S)


def runtime_harness(fixture, entries: list[dict]) -> str:
    # Retain the independent original-PPC fixture's input, aliasing and full
    # context/memory comparison; invoke the linked production wrapper instead.
    source = fixture.harness(entries).decode("utf-8")
    start_marker = {
        pointers: "    PointerFieldRegisters registers = CopyRegisters(recovered);",
        assignments: "    GlobalAssignmentRegisters registers = CopyRegisters(recovered);",
        operations: "    auto registers = CopyRegisters(recovered);",
    }[fixture]
    start = source.index(start_marker)
    final_marker = "    CopyRegisters(recovered, registers);"
    end = source.index(final_marker, start) + len(final_marker)
    source = source[:start] + "    RunWrapper(entry.address, scenario, recovered, recovered_space.bytes);" + source[end:]
    source = source[:source.index("int main()")]
    table = {pointers: "kPointerFieldEntries", assignments: "kGlobalAssignmentEntries",
             operations: "kEntries"}[fixture]
    source += f"""
int main()
{{
    Space original, recovered;
    for (const auto& entry : {table})
        for (const unsigned scenario : {{0u, kCases - 1}})
            if (!Test(entry, scenario, original, recovered)) return 1;
    std::printf("PASS %zu object wrappers, direct + table, %s\\n",
        std::size({table}),
        lo::runtime::semantic_objects::Enabled() ? "enabled" : "disabled");
}}
"""
    declarations = ['#include "cpu/semantic_objects.h"', '#include <stdexcept>',
                    'extern PPCFunc* DirectCalls[];', 'extern unsigned g_original_calls[];']
    declarations.extend(f'extern "C" PPC_FUNC(__imp__sub_{entry["address"]});' for entry in entries)
    declarations.append(f"""
void RunWrapper(std::uint32_t address, unsigned scenario, PPCContext& ctx,
                std::uint8_t* base)
{{
    std::size_t index = 0;
    while (index != {len(entries)} && PPCFuncMappings[index].guest != address) ++index;
    if (index == {len(entries)}) throw std::runtime_error("missing object mapping");
    const unsigned before = g_original_calls[index];
    (scenario == 0 ? DirectCalls[index] : PPCFuncMappings[index].host)(ctx, base);
    const unsigned expected = before + (lo::runtime::semantic_objects::Enabled() ? 0u : 1u);
    if (g_original_calls[index] != expected)
        throw std::runtime_error("object wrapper used the wrong fallback path");
}}
""")
    return "\n".join(declarations) + "\n" + source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-object-runtime-tests")
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
    gate = "LO_SEMANTIC_OBJECT_RUNTIME"
    disabled_env = {key: value for key, value in env.items() if key.upper() != gate}
    results = []
    for family, fixture in (("pointer_fields", pointers), ("global_assignments", assignments),
                            ("field_operations", operations)):
        destination = output / family
        destination.mkdir(exist_ok=True)
        manifest = json.loads(fixture.MAP.read_text(encoding="utf-8"))
        entries = manifest["entries"]
        originals = extract_originals([
            {**entry, "generated_ppc_path": entry.get("generated_ppc_path", entry["source"]),
             "line": entry.get("line", entry["source_line"])} for entry in entries], args.ppc_root)
        assignments.check_originals(entries, originals)
        bodies = [match.group() for match in BODY.finditer(originals)]
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
        results.append(dict(family=family, entries=len(entries), comparisons=len(entries) * 4))
    (output / "object-runtime-result.json").write_text(json.dumps(
        dict(status="passed", batches=results,
             scope="Original PPC full context and seeded ordinary memory; actual direct/table strong wrappers and fallback counts in both gate modes."),
        indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
