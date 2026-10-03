"""Compare the new thread last-error getter and its real tail."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from generate_crt_last_error import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-crt-last-error-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "crt_last_error_families.json").read_text()) != manifest:
        raise ValueError("last-error manifest changed")
    prelude = b"#define sub_822CA108(ctx, base) __imp__sub_822CA108(ctx, base)\n"
    originals = prelude + b"\n".join(e["translated_body"].encode()
        for e in manifest["entries"])
    compile_and_run("crt-last-error", originals,
        (semantics / "tests/crt_last_error_oracle.cpp").read_bytes(),
        [semantics / "src/crt_last_error.cpp"], args.output)

if __name__ == "__main__":
    main()
