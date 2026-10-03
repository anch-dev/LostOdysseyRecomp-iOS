"""Compare the pointer-cache flush with its original body at service boundaries."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path,
                        default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-pointer-vector-tests")
    args = parser.parse_args()
    original = extract_originals([{
        "address": "827C4FA0", "line": 8674,
        "generated_ppc_path": "LostOdysseyRecompLib/ppc/ppc_recomp.75.cpp",
    }], args.ppc_root)
    declarations = b'''
PPC_FUNC(__savegprlr_27);
PPC_FUNC(__restgprlr_27);
PPC_FUNC(sub_822958F8);
PPC_FUNC(__imp__RtlLeaveCriticalSection);
void PointerVectorIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) PointerVectorIndirect(ctx, base, address)
'''
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/pointer_vector_oracle.cpp").read_bytes()
    result = compile_and_run("pointer-vector", declarations + original, harness,
        ["LostOdysseyRecompSemantics/src/pointer_vector.cpp",
         "LostOdysseyRecompSemantics/src/manager_lock.cpp"], args.output)
    print(result["summary"])


if __name__ == "__main__":
    main()
