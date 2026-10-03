"""Compare the fixed heap-lock exit with native Windows generated PPC."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_heap_lock_exit import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-heap-lock-exit-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "heap_lock_exit_families.json").read_text()) != manifest:
        raise ValueError("heap-lock exit manifest changed")
    prelude = (b"void OriginalLeave(PPCContext&, std::uint8_t*);\n"
        b"#define __imp__RtlLeaveCriticalSection(ctx, base) OriginalLeave(ctx, base)\n")
    originals = prelude + manifest["entries"][0]["translated_body"].encode()
    compile_and_run("heap-lock-exit", originals,
        (semantics / "tests/heap_lock_exit_oracle.cpp").read_bytes(),
        [semantics / "src/heap_lock_exit.cpp"], args.output)


if __name__ == "__main__":
    main()
