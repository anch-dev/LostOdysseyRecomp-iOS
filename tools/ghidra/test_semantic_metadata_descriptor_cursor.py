"""Compare both cursor bodies with original PPC and a bounded virtual call."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_metadata_descriptor_cursor import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-metadata-descriptor-cursor-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate(args.ppc_root)
    if json.loads((semantics / "metadata_descriptor_cursor_families.json")
                  .read_text(encoding="utf-8")) != manifest:
        raise ValueError("cursor full-body manifest changed")
    prelude = b"""
void OriginalVirtualCall(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(target) OriginalVirtualCall(ctx, base, target)
PPC_FUNC(sub_822A6EF8);
"""
    originals = b"\n".join([prelude] + [
        entry["translated_body"].encode("utf-8")
        for entry in manifest["entries"]])
    compile_and_run("metadata-descriptor-cursor", originals,
        (semantics / "tests/metadata_descriptor_cursor_oracle.cpp").read_bytes(),
        [semantics / "src/metadata_descriptor_cursor.cpp"], args.output)


if __name__ == "__main__":
    main()
