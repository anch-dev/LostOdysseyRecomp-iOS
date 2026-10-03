"""Compare five recovered manager and array facades with original PPC."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-manager-facade-tests")
    args = parser.parse_args()
    locations = [("823F3340", 14, 30680), ("82486C88", 24, 7399),
                 ("823F3548", 14, 31017), ("82298A98", 0, 20834),
                 ("82298938", 0, 20621)]
    entries = [{"address": address, "line": line,
                "generated_ppc_path": f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp"}
               for address, unit, line in locations]
    declarations = b'''
PPC_FUNC(sub_823F3340); PPC_FUNC(sub_82486C88);
PPC_FUNC(sub_823F3548); PPC_FUNC(sub_82298A98);
PPC_FUNC(sub_82298938); PPC_FUNC(sub_827C5F38);
PPC_FUNC(sub_82298AF8); PPC_FUNC(sub_8229F678);
void ManagerFacadeIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) ManagerFacadeIndirect(ctx, base, address)
'''
    original = extract_originals(entries, args.ppc_root)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/manager_facade_oracle.cpp").read_bytes()
    compile_and_run("manager-facade", declarations + original, harness,
                    ["LostOdysseyRecompSemantics/src/manager_facade.cpp",
                     "LostOdysseyRecompSemantics/src/manager_init.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
