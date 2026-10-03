"""Compare the integer sequence layouts with exact cached original PPC."""

from __future__ import annotations

import argparse
from pathlib import Path

from generate_instance_integer_sequence_family import (
    ROOT, MANIFEST, SOURCE, expected_manifest, source_with_table,
)
from generate_instance_vtable_family import raw_bodies
from semantic_batch import compile_and_run


SELECTED = (
    "8240AF30", "8245C678", "824772F8", "824C1328",
    "825E2A98", "825F2860", "825F4450", "826980D0",
    "826A0D28", "826A0EA0", "826E0918", "826F0C38",
)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-integer-sequence-tests")
    args = parser.parse_args()
    # Static admission checks all 14 exact translated bodies and parameters.
    import json
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != expected_manifest() or \
            SOURCE.read_text(encoding="utf-8") != source_with_table():
        raise ValueError("integer sequence source/manifest differs from cached PPC")
    bodies = raw_bodies(ROOT / "out/function-inventory/registered-instance-originals.cpp.gz")
    original = "\n\n".join("\n".join(bodies[address]) for address in SELECTED)
    harness_path = ROOT / "LostOdysseyRecompSemantics/tests/instance_integer_sequence_oracle.cpp"
    harness = harness_path.read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("integer sequence oracle table marker changed")
    table = "\n".join(f"    {{0x{address}u, __imp__sub_{address}}},"
                      for address in SELECTED)
    harness = harness.replace("/* ENTRY_TABLE */", table)
    compile_and_run("instance-integer-sequence", original.encode("utf-8"),
                    harness,
                    ["LostOdysseyRecompSemantics/src/instance_integer_sequence_family.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
