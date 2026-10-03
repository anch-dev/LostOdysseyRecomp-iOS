"""Compare two original PPC string-initializer wrappers with proven lower semantics."""

import argparse
import json
from pathlib import Path

from generate_instance_string_initializer_family import (
    DEFAULT_PPC_ROOT, recover,
)
from semantic_batch import ROOT, compile_and_run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path.home() / "worktrees/LostOdysseyRecomp"
                        / "semantic-instance-string-initializer-tests")
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics"
                           / "instance_string_initializer_families.json")
                          .read_text(encoding="utf-8"))
    if recover(args.ppc_root) != manifest or manifest["entry_count"] != 2:
        raise ValueError("tracked string-initializer body/CFG/parameters changed")
    lower_receipt = (Path.home() / "worktrees/LostOdysseyRecomp"
                     / "semantic-metadata-string-tests"
                     / "registered-metadata-string-result.json")
    if json.loads(lower_receipt.read_text(encoding="utf-8"))["status"] != "passed":
        raise ValueError("lower InitializeString PPC comparison is not passed")
    entries = manifest["entries"]
    original = ("PPC_FUNC(sub_82407300);\nPPC_FUNC(sub_8229C8B0);\n" +
                "\n".join(entry["translated_body"] for entry in entries))
    table = "\n".join(
        f"    {{0x{entry['address']}u, __imp__sub_{entry['address']}}},"
        for entry in entries)
    template = (ROOT / "LostOdysseyRecompSemantics/tests"
                / "instance_string_initializer_oracle.cpp").read_text(encoding="utf-8")
    if template.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("string-initializer oracle table marker changed")
    sources = [
        "LostOdysseyRecompSemantics/src/instance_string_initializer_family.cpp",
        "LostOdysseyRecompSemantics/src/registered_metadata_string.cpp",
        "LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
        "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
        "LostOdysseyRecompSemantics/src/object_registration.cpp",
        "LostOdysseyRecompSemantics/src/manager_facade.cpp",
        "LostOdysseyRecompSemantics/src/manager_init.cpp",
        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
        "LostOdysseyRecompSemantics/src/memory_move.cpp",
    ]
    compile_and_run("instance-string-initializer", original.encode("utf-8"),
                    template.replace("/* ENTRY_TABLE */", table), sources,
                    args.output)


if __name__ == "__main__":
    main()
