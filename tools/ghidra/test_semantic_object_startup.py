"""Compare recovered object startup paths with their original PPC bodies."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-object-startup-tests")
    args = parser.parse_args()
    entries = [{"address": address, "line": line,
                "generated_ppc_path": "LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp"}
               for address, line in (("823F8980", 12348),
                                     ("823F89F8", 12419),
                                     ("823F8A68", 12488),
                                     ("82406B00", 45609))]
    declarations = b'''
PPC_FUNC(sub_823F8980); PPC_FUNC(sub_823F89F8);
PPC_FUNC(sub_827CA048); PPC_FUNC(sub_82410B90); PPC_FUNC(sub_82410C48);
void ObjectStartupIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) ObjectStartupIndirect(ctx, base, address)
'''
    original = extract_originals(entries, args.ppc_root)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/object_startup_oracle.cpp").read_bytes()
    compile_and_run("object-startup", declarations + original, harness,
                    ["LostOdysseyRecompSemantics/src/object_startup.cpp",
                     "LostOdysseyRecompSemantics/src/object_registration.cpp",
                     "LostOdysseyRecompSemantics/src/manager_facade.cpp",
                     "LostOdysseyRecompSemantics/src/manager_init.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
