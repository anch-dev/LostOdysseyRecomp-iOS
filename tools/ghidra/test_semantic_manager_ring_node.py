"""Compare the complete ring/direct-allocation node composition."""

import argparse
import json
from pathlib import Path

from generate_manager_ring_node import reviewed
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-manager-ring-node-tests")
    args = parser.parse_args()
    def manifest(name):
        return json.loads((ROOT / "LostOdysseyRecompSemantics" / name).read_text(encoding="utf-8"))
    own = manifest("manager_ring_node_families.json")
    if own != reviewed(args.source_root):
        raise ValueError("ring node reviewed baseline changed")
    ring = manifest("ring_reservation_families.json")
    data = manifest("instance_manager_link_families.json")
    leaf = next(e for e in data["entries"] if e["address"] == "82700FD8")
    declarations = "\n".join(f"PPC_FUNC({name});" for name in (
        "__savegprlr_28", "__restgprlr_28", "sub_82486C88", "sub_82290AB8", "sub_82700FD8"))
    dispatch = """static void RingNodeIndirect(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) RingNodeIndirect(ctx, base, address)
"""
    bodies = [*own["abi_helpers"], *own["entries"], *ring["entries"], leaf]
    originals = dispatch + declarations + "\n" + "\n".join(e["translated_body"] for e in bodies)
    compile_and_run("manager-ring-node", originals.encode(),
        (ROOT / "LostOdysseyRecompSemantics/tests/manager_ring_node_oracle.cpp")
            .read_text(encoding="utf-8"),
        ["LostOdysseyRecompSemantics/src/" + name + ".cpp" for name in (
            "manager_ring_node", "ring_reservation", "instance_manager_link_family",
            "manager_facade", "manager_init", "allocation_array", "memory_move")], args.output)


if __name__ == "__main__":
    main()
