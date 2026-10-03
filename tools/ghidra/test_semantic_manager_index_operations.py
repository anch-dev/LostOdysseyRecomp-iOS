"""Compare two indexed upsert bodies with original PPC and known lower models."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_manager_index_operations import originals
from semantic_batch import ROOT, compile_and_run


def abi_helper(ppc_root: Path, name: str, line: int,
               expected_instructions: list[str]) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(
        keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper source line changed: {name}")
    end = next(index for index in range(start + 1, start + 30)
               if lines[index].strip() == b"}")
    body = b"".join(lines[start:end + 1])
    instructions = [x.decode("ascii") for x in re.findall(
        rb"^\s*//\s*(.*?)\s*$", body, re.M)]
    if instructions != expected_instructions:
        raise ValueError(f"ABI helper instructions changed: {name}")
    return body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-manager-index-operations-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = json.loads((semantics / "manager_index_operations_families.json")
                          .read_text(encoding="utf-8"))
    entries = originals(args.ppc_root)
    if manifest["entry_count"] != 2 or manifest["entries"] != entries:
        raise ValueError("manager-index operations exact original manifest changed")
    helper_specs = (
        ("savegprlr_28", 5671, [
            "std r28,-40(r1)", "std r29,-32(r1)", "std r30,-24(r1)",
            "std r31,-16(r1)", "stw r12,-8(r1)", "blr"]),
        ("restgprlr_28", 6257, [
            "ld r28,-40(r1)", "ld r29,-32(r1)", "ld r30,-24(r1)",
            "ld r31,-16(r1)", "lwz r12,-8(r1)", "mtlr r12", "blr"]),
        ("savegprlr_29", 5691, [
            "std r29,-32(r1)", "std r30,-24(r1)", "std r31,-16(r1)",
            "stw r12,-8(r1)", "blr"]),
        ("restgprlr_29", 6279, [
            "ld r29,-32(r1)", "ld r30,-24(r1)", "ld r31,-16(r1)",
            "lwz r12,-8(r1)", "mtlr r12", "blr"]),
    )
    helpers = [abi_helper(args.ppc_root, *spec) for spec in helper_specs]
    declarations = b"\n".join(f"PPC_FUNC({name});".encode() for name in (
        "sub_82326B08", "sub_8229F678", "__savegprlr_28",
        "__restgprlr_28", "__savegprlr_29", "__restgprlr_29"))
    bodies = b"\n".join([declarations, *helpers] + [
        entry["translated_body"].encode("utf-8") for entry in entries])
    harness = (semantics / "tests/manager_index_operations_oracle.cpp").read_bytes()
    compile_and_run("manager-index-operations", bodies, harness, [
        "LostOdysseyRecompSemantics/src/manager_index_operations.cpp",
        "LostOdysseyRecompSemantics/src/manager_index_tables.cpp",
        "LostOdysseyRecompSemantics/src/manager_facade.cpp",
        "LostOdysseyRecompSemantics/src/manager_init.cpp",
        "LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
        "LostOdysseyRecompSemantics/src/memory_move.cpp",
    ], args.output)


if __name__ == "__main__":
    main()
