"""Compare three name-index entries against their exact generated PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_metadata_name_index import originals
from semantic_batch import ROOT, compile_and_run


HELPERS = (
    ("savegprlr_27", 5649, [
        "std r27,-48(r1)", "std r28,-40(r1)", "std r29,-32(r1)",
        "std r30,-24(r1)", "std r31,-16(r1)", "stw r12,-8(r1)", "blr"]),
    ("restgprlr_27", 6233, [
        "ld r27,-48(r1)", "ld r28,-40(r1)", "ld r29,-32(r1)",
        "ld r30,-24(r1)", "ld r31,-16(r1)", "lwz r12,-8(r1)",
        "mtlr r12", "blr"]),
    ("savegprlr_29", 5691, [
        "std r29,-32(r1)", "std r30,-24(r1)", "std r31,-16(r1)",
        "stw r12,-8(r1)", "blr"]),
    ("restgprlr_29", 6279, [
        "ld r29,-32(r1)", "ld r30,-24(r1)", "ld r31,-16(r1)",
        "lwz r12,-8(r1)", "mtlr r12", "blr"]),
)


def helper_body(ppc_root: Path, name: str, line: int,
                expected: list[str]) -> bytes:
    lines = (ppc_root / "ppc_recomp.175.cpp").read_bytes().splitlines(
        keepends=True)
    start = line - 1
    if lines[start].strip() != f"PPC_FUNC_IMPL(__imp____{name}) {{".encode():
        raise ValueError(f"ABI helper source line changed: {name}")
    end = next(index for index in range(start + 1, start + 32)
               if lines[index].strip() == b"}")
    body = b"".join(lines[start:end + 1])
    instructions = [value.decode("ascii") for value in re.findall(
        rb"^\s*//\s*(.*?)\s*$", body, re.M)]
    if instructions != expected:
        raise ValueError(f"ABI helper body changed: {name}")
    return body


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-name-index-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = json.loads((semantics / "metadata_name_index_families.json")
                          .read_text(encoding="utf-8"))
    entries = originals(args.ppc_root)
    if manifest["entry_count"] != 3 or manifest["entries"] != entries:
        raise ValueError("metadata-name index exact manifest changed")
    declarations = b"\n".join(f"PPC_FUNC({name});".encode() for name in (
        "sub_82296FE8", "sub_82296F68", "sub_8229F678",
        "__savegprlr_27", "__restgprlr_27",
        "__savegprlr_29", "__restgprlr_29"))
    bodies = b"\n".join([declarations] + [
        helper_body(args.ppc_root, *helper) for helper in HELPERS] + [
        entry["translated_body"].encode("utf-8") for entry in entries])
    harness = (semantics / "tests/metadata_name_index_oracle.cpp").read_bytes()
    compile_and_run("metadata-name-index", bodies, harness, [
        "LostOdysseyRecompSemantics/src/metadata_name_index.cpp",
        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
        "LostOdysseyRecompSemantics/src/memory_move.cpp",
    ], args.output)


if __name__ == "__main__":
    main()
