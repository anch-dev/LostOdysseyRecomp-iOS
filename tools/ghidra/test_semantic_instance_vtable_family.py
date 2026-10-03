"""Compare representative instance initializers against exact cached PPC bodies."""

from __future__ import annotations

import argparse
import gzip
import json
from pathlib import Path
import re

from semantic_batch import ROOT, compile_and_run


SAMPLES = {
    "vtable_only": ("82405B68", "8240A6C0", "8245D138", "8255DB40", "8272FCA8"),
    "ordered_fields": ("8240A620", "825F6FF0", "82656538", "82657828",
                       "826587A0", "8266EA50"),
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-vtable-tests")
    parser.add_argument("--group", choices=SAMPLES, default="ordered_fields")
    args = parser.parse_args()

    entries = json.loads((ROOT /
        "LostOdysseyRecompSemantics/instance_vtable_families.json")
        .read_text(encoding="utf-8"))["entries"]
    if len(entries) != 583 or len({entry["address"] for entry in entries}) != 583:
        raise ValueError("instance vtable manifest changed")
    by_address = {entry["address"]: entry for entry in entries}
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
    for address in SAMPLES[args.group]:
        entry = by_address[address]
        if entry["kind"] != args.group:
            raise ValueError(f"instance initializer kind changed: {address}")
        body = bodies[address]
        if not body.startswith(f"PPC_FUNC_IMPL(__imp__sub_{address}) {{".encode()):
            raise ValueError(f"original body mismatch: {address}")
        originals.append(body)
        table.append(f"    {{0x{address.lower()}u, {entry['vtable'].lower()}u, "
                     f"{entry['field_offset']}u, "
                     f"{entry['final_field_word'].lower()}u, "
                     f"__imp__sub_{address}}},")
    harness = (ROOT /
        "LostOdysseyRecompSemantics/tests/instance_vtable_family_oracle.cpp"
        ).read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("instance oracle table marker changed")
    suite = ("instance-vtable-family" if args.group == "vtable_only"
             else "instance-vtable-ordered")
    compile_and_run(
        suite, b"\n".join(originals),
        harness.replace("/* ENTRY_TABLE */", "\n".join(table)),
        ["LostOdysseyRecompSemantics/src/instance_vtable_family.cpp"],
        args.output)


if __name__ == "__main__":
    main()
