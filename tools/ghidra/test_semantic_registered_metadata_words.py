"""Compare the 18 new metadata-word entry bodies with recovered semantics."""

import argparse
import json
from pathlib import Path

from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-registered-metadata-tests")
    parser.add_argument("--scope", choices=("all", "negative-growth"),
                        default="all")
    args = parser.parse_args()
    cache = ROOT / "out/function-inventory"
    add = json.loads((cache / "registered-array-add.json").read_text(encoding="utf-8"))
    append = json.loads((cache / "registered-token-append.json").read_text(encoding="utf-8"))
    candidates = json.loads((cache / "registered-extra-methods.json")
                            .read_text(encoding="utf-8"))
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/registered_metadata_word_families.json")
                          .read_text(encoding="utf-8"))
    entries = manifest["entries"]
    reviewed = [candidate for candidate in candidates
                if (calls := [line for line in candidate["instructions"]
                             if line.startswith("bl 0x")]) and
                all(call == "bl 0x825f41e8" for call in calls)]
    addresses = [entry["address"] for entry in entries]
    if (manifest["schema_version"] != 1 or manifest["family"] != "registered_metadata_word" or
            manifest["entry_count"] != 16 or len(entries) != 16 or
            addresses != sorted(set(addresses)) or
            addresses != sorted(candidate["address"] for candidate in reviewed)):
        raise ValueError("reviewed metadata callback set changed")
    if (add["address"] != "822C42D8" or len(add["instructions"]) != 27 or
            append["address"] != "825F41E8" or len(append["instructions"]) != 26):
        raise ValueError("metadata helper cache changed")
    by_address = {candidate["address"]: candidate for candidate in reviewed}
    for entry in entries:
        cached = by_address[entry["address"]]
        if (entry["source"] != cached["generated_ppc_path"] or
                entry["source_line"] != cached["line"] or
                len(entry["words"]) != sum(line == "bl 0x825f41e8"
                                           for line in cached["instructions"])):
            raise ValueError(f"metadata source/sequence changed: {entry['address']}")
    bodies = [add, append, *(by_address[address] for address in addresses)]
    for item in bodies:
        if not item["body"].startswith(
                f"PPC_FUNC_IMPL(__imp__sub_{item['address']})"):
            raise ValueError(f"metadata body changed: {item['address']}")
    declarations = ["PPC_FUNC(sub_8229F678);", "PPC_FUNC(sub_822C42D8);",
                    "PPC_FUNC(sub_825F41E8);"]
    original_cpp = "\n".join([*declarations, *(item["body"] for item in bodies)])
    table = "\n".join(
        f"    {{0x{entry['address']}u, {entry['frame_size']}u, "
        f"{len(entry['words'])}u, {entry['patch_previous_index']}, "
        f"{entry['clear_object_offset']}u, __imp__sub_{entry['address']}}},"
        for entry in entries)
    template = (ROOT / "LostOdysseyRecompSemantics/tests/registered_metadata_words_oracle.cpp")
    harness = template.read_text(encoding="utf-8")
    if harness.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("metadata oracle table marker changed")
    if args.scope == "negative-growth":
        harness = "#define LO_METADATA_NEGATIVE_GROWTH_ONLY\n" + harness
    compile_and_run("registered-metadata-negative-growth" if
                    args.scope == "negative-growth" else "registered-metadata-words",
                    original_cpp.encode("utf-8"),
                    harness.replace("/* ENTRY_TABLE */", table),
                    ["LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp"], args.output)


if __name__ == "__main__":
    main()
