"""Compare a composed CRT thread-record lifecycle with original PPC calls."""

import argparse
from pathlib import Path

from semantic_batch import ROOT, compile_and_run, extract_originals


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=ROOT / "LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-crt-lifecycle-tests")
    args = parser.parse_args()
    locations = [("822CA048", 2, 40673), ("822CA100", 2, 40788),
                 ("822CA108", 2, 40799), ("822CA128", 2, 40824),
                 ("822CA180", 2, 40883), ("822CA188", 2, 40894),
                 ("82290AA8", 0, 1639), ("82B81778", 176, 3873),
                 ("82B816A0", 176, 3740), ("823ADDC0", 11, 29684),
                 ("823ACC98", 11, 27167), ("82B7FD78", 175, 21172),
                 ("82B7FE68", 176, 3), ("82B7FD10", 175, 21105)]
    entries = [{"address": address, "line": line,
                "generated_ppc_path": f"LostOdysseyRecompLib/ppc/ppc_recomp.{unit}.cpp"}
               for address, unit, line in locations]
    declarations = b'''
PPC_FUNC(sub_822CA100); PPC_FUNC(sub_822CA108);
PPC_FUNC(sub_822CA128); PPC_FUNC(sub_822CA180);
PPC_FUNC(sub_822CA188); PPC_FUNC(sub_82290AA8);
PPC_FUNC(sub_82B81778); PPC_FUNC(sub_82B816A0);
PPC_FUNC(sub_823ADDC0); PPC_FUNC(sub_823ACC98);
PPC_FUNC(sub_82B7FD78); PPC_FUNC(sub_82B7FE68);
PPC_FUNC(sub_82B7FD10); PPC_FUNC(sub_822CA048);
PPC_FUNC(sub_823ACCB0); PPC_FUNC(sub_823ADE28);
PPC_FUNC(sub_82B7FEC0);
PPC_FUNC(__imp__KeTlsGetValue); PPC_FUNC(__imp__KeTlsSetValue);
PPC_FUNC(__savegprlr_29); PPC_FUNC(__restgprlr_29);
PPC_FUNC(__savegprlr_28); PPC_FUNC(__restgprlr_28);
PPC_FUNC(__savegprlr_25); PPC_FUNC(__restgprlr_25);
void CrtLifecycleIndirect(PPCContext&, uint8_t*, uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) CrtLifecycleIndirect(ctx, base, address)
'''
    original = extract_originals(entries, args.ppc_root)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/crt_lifecycle_oracle.cpp").read_bytes()
    compile_and_run("crt-lifecycle", declarations + original, harness,
                    ["LostOdysseyRecompSemantics/src/crt_lifecycle.cpp",
                     "LostOdysseyRecompSemantics/src/crt_thread_data.cpp",
                     "LostOdysseyRecompSemantics/src/crt_allocation.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_failure.cpp",
                     "LostOdysseyRecompSemantics/src/heap_allocate.cpp",
                     "LostOdysseyRecompSemantics/src/heap_free.cpp",
                     "LostOdysseyRecompSemantics/src/heap.cpp",
                     "LostOdysseyRecompSemantics/src/memory_fill.cpp",
                     "LostOdysseyRecompSemantics/src/memory_services.cpp"], args.output)


if __name__ == "__main__":
    main()
