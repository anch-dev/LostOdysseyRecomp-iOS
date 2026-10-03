"""One bounded native comparison of six reviewed UTF-16/metadata entries."""

import argparse
import json
from pathlib import Path

from generate_registered_metadata_string import SOURCES, extract
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-metadata-string-tests")
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="checkout containing exact generated PPC source")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics/"
                           "registered_metadata_string_families.json")
                          .read_text(encoding="utf-8"))
    entries = manifest["entries"]
    addresses = [entry["address"] for entry in entries]
    if (manifest["schema_version"] != 1 or
            manifest["family"] != "registered_metadata_string" or
            manifest["entry_count"] != 6 or len(entries) != 6 or
            addresses != sorted(set(addresses))):
        raise ValueError("reviewed six-entry manifest changed")
    if entries != [extract(address, *source, args.source_root)
                   for address, source in SOURCES.items()]:
        raise ValueError("complete translated body or CFG differs from reviewed PPC")
    declarations = "\n".join(f"PPC_FUNC(sub_{address});" for address in
                             ["82296830", "8229C8B0", "8229F5E0", "8229F678",
                              "82B7A0B0", "822C42D8", "825F41E8", "8240B1B8"])
    original = declarations + "\n" + "\n".join(
        entry["translated_body"] for entry in entries)
    harness = (ROOT / "LostOdysseyRecompSemantics/tests/"
               "registered_metadata_string_oracle.cpp").read_text(encoding="utf-8")
    compile_and_run("registered-metadata-string", original.encode("utf-8"),
                    harness,
                    ["LostOdysseyRecompSemantics/src/registered_metadata_string.cpp",
                     "LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
                     "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
                     "LostOdysseyRecompSemantics/src/object_registration.cpp",
                     "LostOdysseyRecompSemantics/src/manager_facade.cpp",
                     "LostOdysseyRecompSemantics/src/manager_init.cpp",
                     "LostOdysseyRecompSemantics/src/allocation_array.cpp",
                     "LostOdysseyRecompSemantics/src/memory_move.cpp"],
                    args.output)


if __name__ == "__main__":
    main()
