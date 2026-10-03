"""Compare three metadata field initializers with their cached original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_registered_metadata_initializers import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-initializer-tests")
    parser.add_argument("--scope", choices=("bounded", "snan"), default="bounded")
    args = parser.parse_args()
    candidates = json.loads((ROOT / "out/function-inventory/registered-extra-methods.json")
                            .read_text(encoding="utf-8"))
    expected = generate(candidates)
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/registered_metadata_initializer_families.json")
                          .read_text(encoding="utf-8"))
    if manifest != expected or manifest["entry_count"] != 3:
        raise ValueError("initializer manifest differs from exact cached bodies")
    by_address = {item["address"]: item for item in candidates}
    entries = manifest["entries"]
    originals = "\n".join(by_address[item["address"]]["body"]
                          for item in entries)
    rows = "\n".join("    {0x" + item["address"] + "u, __imp__sub_" +
                     item["address"] + "}," for item in entries)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/registered_metadata_initializers_oracle.cpp")
    text = harness.read_text(encoding="utf-8")
    if text.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("initializer oracle table marker changed")
    if args.scope == "snan":
        text = "#define LO_METADATA_SNAN_ONLY\n" + text
    suite = ("registered-metadata-initializers-snan" if args.scope == "snan"
             else "registered-metadata-initializers")
    result = compile_and_run(suite, originals.encode("utf-8"),
                             text.replace("/* ENTRY_TABLE */", rows),
                             ["LostOdysseyRecompSemantics/src/registered_metadata_initializers.cpp"],
                             args.output)
    if args.scope == "snan":
        result.update(status="known_mismatch_reproduced", semantic_equivalence=False)
        (args.output.resolve() / f"{suite}-result.json").write_text(
            json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
