"""Compare three CRT stream-error entry bodies with original PPC."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_crt_stream_error import MANIFEST, ROOT, generate
from semantic_batch import compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-crt-stream-error-tests")
    args = parser.parse_args()
    manifest = generate(args.ppc_root)
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != manifest:
        raise ValueError("CRT stream-error manifest differs")
    prelude = b"""
void OriginalThread(PPCContext&, std::uint8_t*);
void OriginalErrorAddress(PPCContext&, std::uint8_t*);
void OriginalInvalid(PPCContext&, std::uint8_t*);
void OriginalTranslate(PPCContext&, std::uint8_t*);
#define sub_822CA048(ctx, base) OriginalThread(ctx, base)
#define sub_82B7FDB0(ctx, base) __imp__sub_82B7FDB0(ctx, base)
#define sub_82B7FD78(ctx, base) OriginalErrorAddress(ctx, base)
#define sub_82B7FEC0(ctx, base) OriginalInvalid(ctx, base)
#define sub_82B7FD10(ctx, base) OriginalTranslate(ctx, base)
"""
    originals = b"\n".join([prelude] + [
        entry["translated_body"].encode("utf-8")
        for entry in manifest["entries"]])
    semantics = ROOT / "LostOdysseyRecompSemantics"
    names = ("crt_stream_error", "crt_thread_data", "allocation_failure",
        "invalid_parameter", "crt_allocation", "memory_services",
        "heap_allocate", "heap_free", "heap_segment", "heap",
        "memory_fill")
    compile_and_run("crt-stream-error", originals,
        (semantics / "tests/crt_stream_error_oracle.cpp").read_bytes(),
        [semantics / f"src/{name}.cpp" for name in names], args.output)


if __name__ == "__main__":
    main()
