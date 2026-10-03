"""Compare five navigation allocation entries with exact original PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_navigation_allocation_family import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-navigation-allocation-tests")
    parser.add_argument("--scope", choices=("base", "r30"), default="base")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "instance_navigation_allocation_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("navigation allocation manifest changed")
    declarations = """
PPC_FUNC(__savegprlr_27); PPC_FUNC(__restgprlr_27);
PPC_FUNC(sub_823B8728); PPC_FUNC(sub_825AEF38);
PPC_FUNC(sub_825AF048); PPC_FUNC(sub_825E12A0);
PPC_FUNC(sub_82486C88);
void NavigationIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) NavigationIndirect(ctx, base, address)
"""
    helpers = (ROOT / "out/function-inventory/allocation-composed-save27.cpp")\
        .read_text(encoding="utf-8")
    originals = declarations + helpers + "\n" + "\n".join(
        entry["translated_body"] for entry in manifest["entries"])
    harness = (semantics / "tests/instance_navigation_allocation_oracle.cpp")\
        .read_text(encoding="utf-8")
    base_cases = [
        "{0x823b8728u, __imp__sub_823B8728, Mode::LinkPlain},",
        "{0x823b8728u, __imp__sub_823B8728, Mode::LinkAlias},",
        "{0x823b8728u, __imp__sub_823B8728, Mode::LinkCallbacks},",
        "{0x825aef38u, __imp__sub_825AEF38, Mode::NodeInitializer},",
        "{0x825af048u, __imp__sub_825AF048, Mode::Allocation},",
        "{0x825e12a0u, __imp__sub_825E12A0, Mode::OwnerReady},",
        "{0x825e12a0u, __imp__sub_825E12A0, Mode::OwnerSuccess},",
        "{0x825e2b08u, __imp__sub_825E2B08, Mode::TailNull},",
        "{0x825e2b08u, __imp__sub_825E2B08, Mode::TailFailure},",
    ]
    selected = base_cases if args.scope == "base" else [
        "{0x825af048u, __imp__sub_825AF048, Mode::AllocationMutateR30},"
    ]
    if harness.count("/* CASE_TABLE */") != 1:
        raise ValueError("navigation oracle case marker changed")
    harness = harness.replace("/* CASE_TABLE */",
                              "\n".join("    " + case for case in selected))
    sources = [
        "LostOdysseyRecompSemantics/src/instance_navigation_allocation_family.cpp",
        "LostOdysseyRecompSemantics/src/manager_facade.cpp",
        "LostOdysseyRecompSemantics/src/manager_init.cpp",
        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
        "LostOdysseyRecompSemantics/src/memory_move.cpp",
    ]
    suite = "instance-navigation-allocation" + ("-r30" if args.scope == "r30" else "")
    compile_and_run(suite, originals.encode("utf-8"),
                    harness, sources, args.output)


if __name__ == "__main__":
    main()
