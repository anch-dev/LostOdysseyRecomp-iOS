"""Compare four stream-pointer unlock entries with original PPC bodies."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_crt_stream_pointer_unlock import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-crt-stream-pointer-unlock-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "crt_stream_pointer_unlock_families.json").read_text()) != manifest:
        raise ValueError("stream-pointer unlock manifest changed")
    prelude = (b"void OriginalNative(PPCContext&, std::uint8_t*);\n"
        b"#define __imp__RtlLeaveCriticalSection(ctx, base) OriginalNative(ctx, base)\n"
        b"#define sub_82B863F0(ctx, base) __imp__sub_82B863F0(ctx, base)\n")
    originals = prelude + b"\n".join(
        entry["translated_body"].encode() for entry in manifest["entries"])
    compile_and_run("crt-stream-pointer-unlock", originals,
        (semantics / "tests/crt_stream_pointer_unlock_oracle.cpp").read_bytes(),
        [semantics / "src/crt_stream_pointer_unlock.cpp"], args.output)


if __name__ == "__main__":
    main()
