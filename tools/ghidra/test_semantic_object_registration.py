"""Compare four readable object construction and registration functions with PPC."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-object-registration-tests")
    args = parser.parse_args()
    locations = [("8240CC58", 16, 8192), ("82410A28", 16, 17327),
                 ("82410B90", 16, 17519), ("82410C48", 16, 17625)]
    entries = [{"address": address, "line": line,
                "generated_ppc_path": f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp"}
               for address, unit, line in locations]
    declarations = b'''
PPC_FUNC(sub_8240CC58); PPC_FUNC(sub_82410A28);
PPC_FUNC(sub_82410B90); PPC_FUNC(sub_82410C48);
PPC_FUNC(sub_82486C88); PPC_FUNC(sub_824059D8);
PPC_FUNC(sub_82408438); PPC_FUNC(sub_824084F0);
void ObjectRegistrationIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) ObjectRegistrationIndirect(ctx, base, address)
'''
    original = extract_originals(entries, args.ppc_root)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/object_registration_oracle.cpp").read_bytes()
    compile_and_run("object-registration", declarations + original, harness,
                    ["LostOdysseyRecompSemantics/src/object_registration.cpp",
                     "LostOdysseyRecompSemantics/src/manager_facade.cpp",
                     "LostOdysseyRecompSemantics/src/manager_init.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
