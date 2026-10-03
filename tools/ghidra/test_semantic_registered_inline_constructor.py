"""Compare recovered 827CE240 with its cached generated PPC body."""

import argparse
import json
from pathlib import Path

from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-registered-inline-tests")
    args = parser.parse_args()
    candidates = json.loads((ROOT / "out/function-inventory/registered-next-dependencies.json")
                            .read_text(encoding="utf-8"))
    entry = next(item for item in candidates if item["address"] == "827CE240")
    if entry["generated_ppc_path"] != "LostOdysseyRecompLib/ppc/ppc_recomp.76.cpp" or \
            len(entry["instructions"]) != 50 or \
            not entry["body"].startswith("PPC_FUNC_IMPL(__imp__sub_827CE240)"):
        raise ValueError("inline constructor PPC cache changed")
    original = "\n".join([
        "PPC_FUNC(__savegprlr_29); PPC_FUNC(__restgprlr_29);",
        "PPC_FUNC(sub_827C5F38); PPC_FUNC(sub_82410A28);",
        "void OriginalAllocate(PPCContext&, uint8_t*, uint32_t);",
        "#undef PPC_CALL_INDIRECT_FUNC",
        "#define PPC_CALL_INDIRECT_FUNC(x) OriginalAllocate(ctx, base, x)",
        entry["body"],
    ]).encode("utf-8")
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/registered_inline_constructor_oracle.cpp"
               ).read_text(encoding="utf-8")
    compile_and_run(
        "registered-inline-constructor", original, harness,
        ["LostOdysseyRecompSemantics/src/registered_inline_constructor.cpp",
         "LostOdysseyRecompSemantics/src/object_registration.cpp",
         "LostOdysseyRecompSemantics/src/manager_facade.cpp",
         "LostOdysseyRecompSemantics/src/manager_init.cpp",
         "LostOdysseyRecompSemantics/src/allocation_array.cpp",
         "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
