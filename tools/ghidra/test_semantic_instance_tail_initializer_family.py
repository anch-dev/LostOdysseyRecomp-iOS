"""Compare five newly closed instance tail paths with their cached PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_tail_initializer_family import PARENTS, STAGED, generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-tail-tests")
    args = parser.parse_args()

    manifest, expected_source = generate()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    if json.loads((semantics / "instance_tail_initializer_families.json")
                  .read_text(encoding="utf-8")) != manifest or \
            (semantics / "src/instance_tail_initializer_family.cpp")\
                    .read_text(encoding="utf-8") != expected_source:
        raise ValueError("tail manifest/source differs from strict cached body")
    entries = {entry["address"]: entry for entry in manifest["entries"]}
    if len(entries) != 9:
        raise ValueError("tail entry count changed")

    declarations = "\n".join(f"PPC_FUNC(sub_{address});" for address in
                             sorted(set(PARENTS.values()) |
                                    {"82B7BC40", "8229F678"}))
    # The already recovered 825A7F18 is included as an exact lower body for
    # this narrow composition, without rerunning its old family suite.
    cached = json.loads((ROOT / "out/function-inventory/instance-tail-callees.json")
                        .read_text(encoding="utf-8"))
    callee_bodies = {entry["address"]: entry["body"] for entry in cached["callees"]}
    originals = "\n".join([declarations] + ["\n".join(callee_bodies[address])
        for address in sorted(set(PARENTS.values()))] +
        ["\n".join(entries[address]["body"]) for address in sorted(PARENTS)])
    table = "\n".join(f"    {{0x{address.lower()}u, __imp__sub_{address}, "
                      f"{'true' if callee in STAGED else 'false'}}},"
                      for address, callee in sorted(PARENTS.items()))
    harness = (semantics / "tests/instance_tail_initializer_family_oracle.cpp")
    text = harness.read_text(encoding="utf-8")
    if text.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("tail oracle entry marker changed")
    compile_and_run("instance-tail-initializer", originals.encode("utf-8"),
                    text.replace("/* ENTRY_TABLE */", table),
                    ["LostOdysseyRecompSemantics/src/instance_tail_initializer_family.cpp",
                     "LostOdysseyRecompSemantics/src/memory_fill.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp",
                     "LostOdysseyRecompSemantics/src/pointer_fields.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
