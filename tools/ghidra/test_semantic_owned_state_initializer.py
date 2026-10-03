"""Compare the two original PPC owned-state bodies with recovered semantics."""

import argparse
import json
from pathlib import Path

from generate_owned_state_initializer import DEFAULT_PPC_ROOT, recover
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path,
                        default=Path.home() / "worktrees/LostOdysseyRecomp"
                        / "semantic-owned-state-tests")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics"
                           / "owned_state_initializer_families.json")
                          .read_text(encoding="utf-8"))
    if recover(args.ppc_root) != manifest or manifest["entry_count"] != 2:
        raise ValueError("tracked exact owned-state PPC bodies changed")
    declarations = """
PPC_FUNC(sub_82486C88); PPC_FUNC(sub_82406A38);
void OwnedStateIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) OwnedStateIndirect(ctx, base, address)
"""
    original = declarations + "\n".join(
        entry["translated_body"] for entry in manifest["entries"])
    harness = (ROOT / "LostOdysseyRecompSemantics/tests"
               / "owned_state_initializer_oracle.cpp").read_text(encoding="utf-8")
    compile_and_run("owned-state", original.encode("utf-8"), harness, [
        "LostOdysseyRecompSemantics/src/owned_state_initializer.cpp",
        "LostOdysseyRecompSemantics/src/manager_facade.cpp",
        "LostOdysseyRecompSemantics/src/manager_init.cpp",
        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
        "LostOdysseyRecompSemantics/src/memory_move.cpp",
    ], args.output)


if __name__ == "__main__":
    main()
