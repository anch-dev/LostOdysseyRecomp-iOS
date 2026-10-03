"""Compare four descriptor-index entries against exact PPC and lower models."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_metadata_descriptor_index import originals
from semantic_batch import ROOT, compile_and_run


HELPERS = (
    ("savegprlr_28", 5671, ["std r28,-40(r1)", "std r29,-32(r1)",
        "std r30,-24(r1)", "std r31,-16(r1)", "stw r12,-8(r1)", "blr"]),
    ("restgprlr_28", 6257, ["ld r28,-40(r1)", "ld r29,-32(r1)",
        "ld r30,-24(r1)", "ld r31,-16(r1)", "lwz r12,-8(r1)",
        "mtlr r12", "blr"]),
    ("savegprlr_29", 5691, ["std r29,-32(r1)", "std r30,-24(r1)",
        "std r31,-16(r1)", "stw r12,-8(r1)", "blr"]),
    ("restgprlr_29", 6279, ["ld r29,-32(r1)", "ld r30,-24(r1)",
        "ld r31,-16(r1)", "lwz r12,-8(r1)", "mtlr r12", "blr"]),
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
        "worktrees/LostOdysseyRecomp/semantic-metadata-descriptor-index-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = json.loads((semantics /
        "metadata_descriptor_index_families.json").read_text(encoding="utf-8"))
    entries = originals(args.ppc_root)
    if manifest["entry_count"] != 4 or manifest["entries"] != entries:
        raise ValueError("descriptor-index exact manifest changed")
    prelude = b"""
PPC_FUNC(__savegprlr_28);
PPC_FUNC(__restgprlr_28);
PPC_FUNC(__savegprlr_29);
PPC_FUNC(__restgprlr_29);
PPC_FUNC(sub_823F3340);
PPC_FUNC(sub_82486C88);
PPC_FUNC(sub_822C42D8);
PPC_FUNC(sub_82523C48);
PPC_FUNC(sub_8256B910);
PPC_FUNC(sub_826BD860);
"""
    helpers = b"\n".join(helper_body(args.ppc_root, *helper)
                         for helper in HELPERS)
    wrappers = b"""
PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
"""
    bodies = b"\n".join([prelude, helpers, wrappers] + [
        entry["translated_body"].encode("utf-8") for entry in entries])
    harness = (semantics / "tests/metadata_descriptor_index_oracle.cpp")\
        .read_bytes()
    compile_and_run("metadata-descriptor-index", bodies, harness, [
        semantics / "src/metadata_descriptor_index.cpp",
        semantics / "src/manager_facade.cpp",
        semantics / "src/manager_init.cpp",
        semantics / "src/registered_metadata_words.cpp",
        semantics / "src/allocation_array.cpp",
        semantics / "src/memory_move.cpp",
    ], args.output)


if __name__ == "__main__":
    main()
