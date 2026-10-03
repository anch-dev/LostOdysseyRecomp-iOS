"""Compare CRT invalid-parameter dispatch with extracted PPC bodies."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-invalid-parameter-tests")
    args = parser.parse_args()
    entries = [{"address": address, "line": line,
                "generated_ppc_path": "LostOdysseyRecompLib/ppc/ppc_recomp.176.cpp"}
               for address, line in [("82B7FEC0", 65), ("82B84D88", 12414)]]
    declarations = b'''
PPC_FUNC(sub_82B84D88);
void InvalidParameterIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) InvalidParameterIndirect(ctx, base, address)
'''
    original = extract_originals(entries, args.ppc_root)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/invalid_parameter_oracle.cpp").read_bytes()
    compile_and_run("invalid-parameter", declarations + original, harness,
                    ["LostOdysseyRecompSemantics/src/invalid_parameter.cpp"], args.output)


if __name__ == "__main__":
    main()
