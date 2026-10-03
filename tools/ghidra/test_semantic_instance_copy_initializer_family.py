"""Compare the copy initializer with its cached parent and actual original copy body."""

from __future__ import annotations

import argparse
import gzip
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-copy-tests")
    parser.add_argument("--copy-fixture", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-memory-move-tests/memory_move_oracle_generated.cpp")
    args = parser.parse_args()
    fixture = args.copy_fixture.read_bytes()
    marker = b"PPC_FUNC(oracle_CopyGuestMemory) {"
    start = fixture.index(marker)
    end = fixture.index(b"\n}", start) + 2
    copy = fixture[start:end].replace(marker, b"PPC_FUNC(sub_82B7A0B0) {", 1)
    if b"sub_" in copy.split(b"{", 1)[1] or re.search(rb"ctx\.r(?:30|31)\.", copy):
        raise ValueError("cached copy body gained calls or nonvolatile effects")
    raw = gzip.decompress((ROOT /
        "out/function-inventory/registered-instance-originals.cpp.gz").read_bytes())
    parts = re.split(rb"(?=PPC_FUNC_IMPL\(__imp__sub_[0-9A-F]{8}\) \{)", raw)
    parents = [part.strip() for part in parts if part.startswith(
        b"PPC_FUNC_IMPL(__imp__sub_8268CC28) {")]
    if len(parents) != 1:
        raise ValueError("cached copy parent missing or duplicated")
    compile_and_run("instance-copy-initializer", b"\n".join([copy, parents[0]]),
        (ROOT / "LostOdysseyRecompSemantics/tests/instance_copy_initializer_oracle.cpp")
        .read_text(encoding="utf-8"),
        ["LostOdysseyRecompSemantics/src/instance_copy_initializer_family.cpp",
         "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
