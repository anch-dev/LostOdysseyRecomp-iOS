"""Compare one cached original PPC body per UI field-layout variant."""

from __future__ import annotations

import argparse
import gzip
import json
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run


SAMPLES = ("82631AD0", "826320A0", "82632480", "82632648")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-ui-tests")
    args = parser.parse_args()

    entries = json.loads((ROOT /
        "LostOdysseyRecompSemantics/instance_ui_initializer_families.json")
        .read_text(encoding="utf-8"))["entries"]
    if len(entries) != 6 or len({entry["address"] for entry in entries}) != 6:
        raise ValueError("UI initializer manifest changed")
    by_address = {entry["address"]: entry for entry in entries}
    layouts = {(by_address[address]["first_field_offset"],
                by_address[address]["zero_field_offset"])
               for address in SAMPLES}
    if layouts != {(656, 660), (608, 612), (608, 752), (608, 748)}:
        raise ValueError("UI layout representative mapping changed")
    raw = gzip.decompress((ROOT /
        "out/function-inventory/registered-instance-originals.cpp.gz").read_bytes())
    parts = re.split(rb"(?=PPC_FUNC_IMPL\(__imp__sub_[0-9A-F]{8}\) \{)", raw)
    bodies = {}
    for part in parts:
        if not part.strip():
            continue
        match = re.match(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{", part)
        if match is None:
            raise ValueError("cached PPC body boundary changed")
        bodies[match.group(1).decode("ascii")] = part.strip()
    table = []
    originals = []
    for address in SAMPLES:
        entry = by_address[address]
        body = bodies[address]
        if not body.startswith(f"PPC_FUNC_IMPL(__imp__sub_{address}) {{".encode()):
            raise ValueError(f"original body mismatch: {address}")
        originals.append(body)
        table.append(
            f"    {{0x{address.lower()}u, {entry['first_field_offset']}u, "
            f"{entry['zero_field_offset']}u, {entry['vtable'].lower()}u, "
            f"{entry['final_field_word'].lower()}u, __imp__sub_{address}}},")
    harness = (ROOT /
        "LostOdysseyRecompSemantics/tests/instance_ui_initializer_family_oracle.cpp"
        ).read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("UI oracle table marker changed")
    compile_and_run(
        "instance-ui-initializer", b"\n".join(originals),
        harness.replace("/* ENTRY_TABLE */", "\n".join(table)),
        ["LostOdysseyRecompSemantics/src/instance_ui_initializer_family.cpp"],
        args.output)


if __name__ == "__main__":
    main()
