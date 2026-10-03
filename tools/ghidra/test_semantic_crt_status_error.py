"""Compare the status-conversion wrapper against its complete original PPC."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_crt_status_error import generate
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
        "worktrees/LostOdysseyRecomp/semantic-crt-status-error-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest = generate()
    if json.loads((semantics / "crt_status_error_families.json").read_text()) != manifest:
        raise ValueError("status-error manifest changed")
    originals = (b"void OriginalConvert(PPCContext&, std::uint8_t*);\n"
        b"#define __imp__RtlNtStatusToDosError(ctx, base) OriginalConvert(ctx, base)\n" +
        manifest["entries"][0]["translated_body"].encode())
    compile_and_run("crt-status-error", originals,
        (semantics / "tests/crt_status_error_oracle.cpp").read_bytes(),
        [semantics / "src/crt_status_error.cpp"], args.output)


if __name__ == "__main__":
    main()
