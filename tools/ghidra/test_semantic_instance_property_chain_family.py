"""Compare nine new property-chain PPC bodies with the real foundation body."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_property_chain_family import generate
from semantic_batch import ROOT, compile_and_run


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path.home() /
                        "worktrees/LostOdysseyRecomp/semantic-instance-property-chain-tests")
    args = parser.parse_args()
    semantics = ROOT / "LostOdysseyRecompSemantics"
    manifest, source = generate()
    if json.loads((semantics / "instance_property_chain_families.json")
                  .read_text(encoding="utf-8")) != manifest or \
            (semantics / "src/instance_property_chain_family.cpp")\
                    .read_text(encoding="utf-8") != source:
        raise ValueError("property chain manifest/source differs from PPC")
    foundation_receipt = (Path.home() / "worktrees/LostOdysseyRecomp"
                          / "semantic-string-property-tests"
                          / "string-property-result.json")
    if json.loads(foundation_receipt.read_text(encoding="utf-8"))[
            "status"] != "passed":
        raise ValueError("string-property foundation oracle not passed")
    foundation = json.loads((semantics / "string_property_initializer_families.json")
                            .read_text(encoding="utf-8"))
    if {entry["address"] for entry in foundation["entries"]} != \
            {"822A06C0", "82496948"}:
        raise ValueError("property foundation changed")

    declarations = "\n".join(f"PPC_FUNC({name});" for name in [
        "__savegprlr_28", "__restgprlr_28", "sub_8229F678",
        "sub_82B7A0B0", "sub_8229C8B0", "sub_82298938",
        "sub_822A06C0", "sub_82496948", "sub_825D5398",
        "sub_825AEEC0", "sub_825AEB30", "sub_826D6F28",
    ])
    originals = declarations + "\n" + "\n".join(
        [entry["translated_body"] for entry in foundation["entries"]] +
        ["\n".join(entry["body"]) for entry in manifest["entries"]])
    cases = []
    for entry in manifest["entries"]:
        address = entry["address"]
        cases.append(f"    {{0x{address.lower()}u, __imp__sub_{address}, "
                     "Mode::Ordinary},")
        if entry["null_guard"]:
            cases.append(f"    {{0x{address.lower()}u, __imp__sub_{address}, "
                         "Mode::Null},")
    cases.append("    {0x825aeb30u, __imp__sub_825AEB30, Mode::StageAlias},")
    template = (semantics / "tests/instance_property_chain_oracle.cpp")\
        .read_text(encoding="utf-8")
    if template.count("/* CASE_TABLE */") != 1:
        raise ValueError("property chain oracle table marker changed")
    sources = [
        "LostOdysseyRecompSemantics/src/instance_property_chain_family.cpp",
        "LostOdysseyRecompSemantics/src/string_property_initializer.cpp",
        "LostOdysseyRecompSemantics/src/registered_metadata_string.cpp",
        "LostOdysseyRecompSemantics/src/registered_metadata_words.cpp",
        "LostOdysseyRecompSemantics/src/registered_constructor_family.cpp",
        "LostOdysseyRecompSemantics/src/object_registration.cpp",
        "LostOdysseyRecompSemantics/src/manager_facade.cpp",
        "LostOdysseyRecompSemantics/src/manager_init.cpp",
        "LostOdysseyRecompSemantics/src/allocation_array.cpp",
        "LostOdysseyRecompSemantics/src/memory_move.cpp",
    ]
    compile_and_run("instance-property-chain", originals.encode("utf-8"),
                    template.replace("/* CASE_TABLE */", "\n".join(cases)),
                    sources, args.output)


if __name__ == "__main__":
    main()
