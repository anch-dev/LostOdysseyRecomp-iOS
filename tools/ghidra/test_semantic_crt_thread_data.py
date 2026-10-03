"""Compare CRT thread acquisition and error output with extracted PPC bodies."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-crt-thread-data-tests")
    args = parser.parse_args()
    locations = [("822CA048", 2, 40673), ("822CA100", 2, 40788),
                 ("822CA108", 2, 40799), ("822CA128", 2, 40824),
                 ("822CA180", 2, 40883), ("822CA188", 2, 40894),
                 ("82290AA8", 0, 1639), ("823ADD70", 11, 29624),
                 ("823ADDB0", 11, 29669)]
    entries = [{"address": address, "line": line,
                "generated_ppc_path": f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp"}
               for address, unit, line in locations]
    declarations = b'''
PPC_FUNC(sub_822CA100);
PPC_FUNC(sub_822CA108);
PPC_FUNC(sub_822CA128);
PPC_FUNC(sub_822CA180);
PPC_FUNC(sub_822CA188);
PPC_FUNC(sub_82290AA8);
PPC_FUNC(sub_823ADDB0);
PPC_FUNC(sub_82B81778);
PPC_FUNC(sub_823ADDC0);
PPC_FUNC(sub_82BECB10);
PPC_FUNC(__imp__KeTlsGetValue);
PPC_FUNC(__imp__KeTlsSetValue);
PPC_FUNC(__imp__RtlInitAnsiString);
PPC_FUNC(__savegprlr_29);
PPC_FUNC(__restgprlr_29);
void CrtIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) CrtIndirect(ctx, base, address)
'''
    original = extract_originals(entries, args.ppc_root)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/crt_thread_data_oracle.cpp").read_bytes()
    compile_and_run("crt-thread-data", declarations + original, harness,
                    ["LostOdysseyRecompSemantics/src/crt_thread_data.cpp"], args.output)


if __name__ == "__main__":
    main()
