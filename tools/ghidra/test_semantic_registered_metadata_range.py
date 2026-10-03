"""Compare five metadata-range entries against their original PPC bodies."""

import argparse
import json
from pathlib import Path

from generate_registered_metadata_range import (
    DEFAULT_PPC_ROOT, HELPER, PARENTS, load_body, recover,
)
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path.home() / "worktrees/LostOdysseyRecomp"
                        / "semantic-metadata-range-tests")
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics"
                           / "registered_metadata_range_families.json")
                          .read_text(encoding="utf-8"))
    if recover(args.ppc_root) != manifest or manifest["entry_count"] != 5:
        raise ValueError("tracked metadata-range evidence changed")
    cached = json.loads((ROOT / "out/function-inventory"
                         / "registered-extra-methods.json").read_text(encoding="utf-8"))
    parents = {entry["address"]: entry for entry in cached
               if entry["address"] in PARENTS}
    add = json.loads((ROOT / "out/function-inventory/registered-array-add.json")
                     .read_text(encoding="utf-8"))
    append = json.loads((ROOT / "out/function-inventory/registered-token-append.json")
                        .read_text(encoding="utf-8"))
    if add["address"] != "822C42D8" or len(add["instructions"]) != 27 or \
            append["address"] != "825F41E8" or len(append["instructions"]) != 26:
        raise ValueError("previously recovered lower helpers changed")
    originals = "\n".join([
        "PPC_FUNC(sub_8229F678);", "PPC_FUNC(sub_822C42D8);",
        "PPC_FUNC(sub_825F41E8);", "PPC_FUNC(sub_8249B130);",
        add["body"], append["body"], load_body(HELPER, args.ppc_root),
        *(parents[address]["body"] for address in sorted(PARENTS)),
    ])
    rows = [f"    {{0x{entry['address']}u, __imp__sub_{entry['address']}}},"
            for entry in manifest["entries"]]
    template = (ROOT / "LostOdysseyRecompSemantics/tests"
                / "registered_metadata_range_oracle.cpp").read_text(encoding="utf-8")
    if template.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("metadata-range oracle table marker changed")
    compile_and_run("metadata-range", originals.encode("utf-8"),
                    template.replace("/* ENTRY_TABLE */", "\n".join(rows)),
                    ["LostOdysseyRecompSemantics/src/registered_metadata_range.cpp",
                     "LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
