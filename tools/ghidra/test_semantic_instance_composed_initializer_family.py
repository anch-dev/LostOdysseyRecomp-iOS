"""Compare eleven closed instance compositions with cached original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_composed_initializer_family import (
    CALLEES, TAILS, WRAPPERS, generate,
)
from semantic_batch import ROOT, compile_and_run


EXTRAS = [
    ("825A80B0", "Null", None),
    ("8262E498", "Null", None),
    ("82716058", "Null", None),
    ("825A56D0", "FlagSet", None),
    ("825A56D0", "AllocateFail", None),
    ("8262E498", "SpillAlias", None),
    ("826DB6A8", "FpAlias", "0x8218952cu"),
    ("826B50C8", "GateReady", None),
    ("826B50C8", "AppendGrowMutate", None),
    ("8272CE60", "AppendGrowMutate", None),
    ("826B50C8", "GateAlias", "0x83313624u"),
]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-composed-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest, expected_source = generate()
    if json.loads((semantics / "instance_composed_initializer_families.json")
                  .read_text(encoding="utf-8")) != manifest or \
            (semantics / "src/instance_composed_initializer_family.cpp")\
                    .read_text(encoding="utf-8") != expected_source:
        raise ValueError("composed manifest/source differs from strict PPC")
    entries = {entry["address"]: entry for entry in manifest["entries"]}
    if len(entries) != 11:
        raise ValueError("composed entry set changed")
    declarations = "\n".join(f"PPC_FUNC(sub_{address});" for address in
                             sorted(CALLEES | {"82486C88", "825F41E8"}))
    originals = "\n".join([declarations] + ["\n".join(entries[address]["body"])
        for address in sorted(entries)])
    cases = [(address, "Basic", None) for address in sorted(entries)] + EXTRAS
    table = "\n".join(f"    {{0x{address.lower()}u, __imp__sub_{address}, "
                      f"Mode::{mode}, {object or 'Object'}}},"
                      for address, mode, object in cases)
    harness = (semantics / "tests/instance_composed_initializer_family_oracle.cpp")
    text = harness.read_text(encoding="utf-8")
    if text.count("/* CASE_TABLE */") != 1:
        raise ValueError("composed oracle marker changed")
    compile_and_run("instance-composed-initializer", originals.encode("utf-8"),
                    text.replace("/* CASE_TABLE */", table),
                    ["LostOdysseyRecompSemantics/src/instance_composed_initializer_family.cpp",
                     "LostOdysseyRecompSemantics/src/manager_facade.cpp",
                     "LostOdysseyRecompSemantics/src/manager_init.cpp",
                     "LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
