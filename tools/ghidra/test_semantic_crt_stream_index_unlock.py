"""Compare both CRT indexed-lock exits with their complete original PPC."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_crt_stream_index_unlock import MANIFEST, ROOT, generate
from semantic_batch import compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-crt-stream-index-unlock-tests")
    args = parser.parse_args()
    manifest = generate()
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != manifest:
        raise ValueError("CRT indexed-lock manifest changed")
    prelude = b"""
extern "C" PPC_FUNC(__imp__sub_82B819C8);
void OriginalLeave(PPCContext&,std::uint8_t*);
#define sub_82B819C8(ctx,base) __imp__sub_82B819C8(ctx,base)
#define __imp__RtlLeaveCriticalSection(ctx,base) OriginalLeave(ctx,base)
"""
    originals = prelude + b"\n".join(item["translated_body"].encode()
        for item in manifest["entries"])
    semantics = ROOT / "LostOdysseyRecompSemantics"
    compile_and_run("crt-stream-index-unlock", originals,
        (semantics / "tests/crt_stream_index_unlock_oracle.cpp").read_bytes(),
        [semantics / "src/crt_stream_index_unlock.cpp"], args.output)


if __name__ == "__main__":
    main()
