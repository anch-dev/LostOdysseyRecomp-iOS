"""Compare member-chain dispatch with the reviewed original PPC body."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_metadata_member_dispatch import MANIFEST, generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-member-dispatch-tests")
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
                        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    manifest = generate(args.ppc_root)
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != manifest:
        raise ValueError("member dispatch manifest changed")
    prelude = """
void OriginalMemberCall(PPCContext&, std::uint8_t*, std::uint32_t);
#undef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(address) OriginalMemberCall(ctx, base, address)
"""
    originals = prelude + manifest["entries"][0]["translated_body"]
    semantics = ROOT / "LostOdysseyRecompSemantics"
    compile_and_run("metadata-member-dispatch", originals.encode("utf-8"),
        (semantics / "tests/metadata_member_dispatch_oracle.cpp").read_bytes(),
        [semantics / "src/metadata_member_dispatch.cpp"], args.output)


if __name__ == "__main__":
    main()
