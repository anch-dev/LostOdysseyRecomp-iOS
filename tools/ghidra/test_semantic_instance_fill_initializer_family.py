"""Compare UI fill compositions with cached original parents and the actual fill body."""

from __future__ import annotations

import argparse
import gzip
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run

SAMPLES = ("8264E920", "82667430", "826674B0")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-fill-tests")
    parser.add_argument("--fill-fixture", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-memory-fill-tests/memory_fill_oracle_generated.cpp")
    args = parser.parse_args()
    fill = args.fill_fixture.read_bytes()
    marker = b"PPC_FUNC(oracle_FillGuestMemory) {"
    start = fill.index(marker)
    end = fill.index(b"\n}", start) + 2
    fill = fill[start:end].replace(marker, b"PPC_FUNC(sub_82B7BC40) {", 1)
    if b"sub_" in fill.split(b"{", 1)[1]:
        raise ValueError("cached fill body gained external dependencies")
    raw = gzip.decompress((ROOT /
        "out/function-inventory/registered-instance-originals.cpp.gz").read_bytes())
    parts = re.split(rb"(?=PPC_FUNC_IMPL\(__imp__sub_[0-9A-F]{8}\) \{)", raw)
    bodies = {}
    for part in parts:
        match = re.match(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{", part)
        if match:
            bodies[match.group(1).decode("ascii")] = part.strip()
    harness = (ROOT /
        "LostOdysseyRecompSemantics/tests/instance_fill_initializer_family_oracle.cpp"
        ).read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("UI fill oracle marker changed")
    table = "\n".join(f"    {{0x{a.lower()}u, __imp__sub_{a}}}," for a in SAMPLES)
    compile_and_run("instance-fill-initializer",
        b"\n".join([fill, *[bodies[a] for a in SAMPLES]]),
        harness.replace("/* ENTRY_TABLE */", table),
        ["LostOdysseyRecompSemantics/src/instance_fill_initializer_family.cpp",
         "LostOdysseyRecompSemantics/src/memory_fill.cpp"], args.output)


if __name__ == "__main__":
    main()
