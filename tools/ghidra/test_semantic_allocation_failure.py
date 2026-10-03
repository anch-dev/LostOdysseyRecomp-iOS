"""Compare allocation-error lookup, reporting, retry and termination boundaries."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-allocation-failure-tests")
    args = parser.parse_args()
    entries = [{"address": address, "line": line,
                "generated_ppc_path": f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp"}
               for address, unit, line in [("82B7FCE0", 175, 21070),
                   ("82B7FC98", 175, 21015), ("82B7BF20", 175, 11384),
                   ("82B7FE68", 176, 3), ("82B7FD78", 175, 21172),
                   ("823ACBD0", 11, 27034), ("823ACC98", 11, 27167)]]
    original = extract_originals(entries, args.ppc_root)
    declarations = b'''
PPC_FUNC(sub_823ADD70);
PPC_FUNC(sub_82B7FC98);
PPC_FUNC(__imp__KeBugCheck);
PPC_FUNC(sub_822CA048);
PPC_FUNC(__savegprlr_28);
PPC_FUNC(__restgprlr_28);
PPC_FUNC(sub_823ACC98);
PPC_FUNC(sub_823ACCB0);
PPC_FUNC(sub_82B7FCE0);
PPC_FUNC(sub_82B7BF20);
PPC_FUNC(sub_82B7FE68);
PPC_FUNC(sub_82B7FD78);
void AllocationFailureIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) AllocationFailureIndirect(ctx, base, address)
'''
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/allocation_failure_oracle.cpp").read_bytes()
    compile_and_run("allocation-failure", declarations + original, harness,
                    ["LostOdysseyRecompSemantics/src/allocation_failure.cpp",
                     "LostOdysseyRecompSemantics/src/raw_allocation.cpp",
                     "LostOdysseyRecompSemantics/src/memory_services.cpp"], args.output)


if __name__ == "__main__":
    main()
