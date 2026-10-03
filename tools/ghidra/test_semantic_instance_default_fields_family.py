"""Compare four ordered default-field representatives with cached PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_default_fields_family import generate
from generate_instance_vtable_family import raw_bodies
from semantic_batch import ROOT, compile_and_run

SAMPLES = ("82412050", "824D87E8", "82632130", "824C3648")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-default-fields-tests")
    args = parser.parse_args()
    cache = ROOT / "out/function-inventory"
    candidates = json.loads((cache / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    bodies = raw_bodies(cache / "registered-instance-originals.cpp.gz")
    expected = generate(candidates, bodies)
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/instance_default_fields_families.json")
                          .read_text(encoding="utf-8"))
    if manifest != expected or manifest["entry_count"] != 42 or \
            manifest["ordered_write_count"] != 464:
        raise ValueError("default-fields manifest differs from exact cached PPC")
    by_address = {entry["address"]: entry for entry in manifest["entries"]}
    if not set(SAMPLES) <= by_address.keys():
        raise ValueError("default-fields oracle representatives changed")
    originals = "\n".join("\n".join(bodies[address]) for address in SAMPLES)
    table = "\n".join(f"    {{0x{address.lower()}u, __imp__sub_{address}}},"
                      for address in SAMPLES)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/instance_default_fields_family_oracle.cpp")
    text = harness.read_text(encoding="utf-8")
    if text.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("default-fields oracle table marker changed")
    compile_and_run("instance-default-fields", originals.encode("utf-8"),
                    text.replace("/* ENTRY_TABLE */", table),
                    ["LostOdysseyRecompSemantics/src/instance_default_fields_family.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
