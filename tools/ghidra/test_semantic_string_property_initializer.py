"""Compare two original PPC string-property bodies with recovered dependencies."""

import argparse
import json
from pathlib import Path

from generate_string_property_initializer import DEFAULT_PPC_ROOT, recover
from semantic_batch import ROOT, compile_and_run


def abi_helper_body(ppc_root, name, line):
    lines = (ppc_root / "ppc_recomp.175.cpp").read_text(
        encoding="utf-8").splitlines()
    if lines[line - 1] != f"PPC_FUNC_IMPL(__imp____{name}) {{":
        raise ValueError(f"recorded ABI helper location changed: {name}")
    end = line
    while end < len(lines) and lines[end] != "}":
        if lines[end].startswith("PPC_FUNC_IMPL("):
            raise ValueError(f"unterminated ABI helper: {name}")
        end += 1
    if end == len(lines):
        raise ValueError(f"unterminated ABI helper: {name}")
    return "\n".join(lines[line - 1:end + 1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path.home() / "worktrees/LostOdysseyRecomp"
                        / "semantic-string-property-tests")
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    args = parser.parse_args()
    manifest = json.loads((ROOT / "LostOdysseyRecompSemantics"
                           / "string_property_initializer_families.json")
                          .read_text(encoding="utf-8"))
    if recover(args.ppc_root) != manifest or manifest["entry_count"] != 2:
        raise ValueError("tracked exact string-property PPC bodies changed")
    lower_receipt = (Path.home() / "worktrees/LostOdysseyRecomp"
                     / "semantic-metadata-string-tests"
                     / "registered-metadata-string-result.json")
    if json.loads(lower_receipt.read_text(encoding="utf-8"))["status"] != "passed":
        raise ValueError("lower InitializeString comparison is not passed")
    declarations = "\n".join(f"PPC_FUNC({name});" for name in [
        "__savegprlr_28", "__restgprlr_28", "sub_8229F678",
        "sub_82B7A0B0", "sub_8229C8B0", "sub_82298938", "sub_822A06C0",
    ])
    original = declarations + "\n" + "\n".join([
        abi_helper_body(args.ppc_root, "savegprlr_28", 5671),
        abi_helper_body(args.ppc_root, "restgprlr_28", 6257),
        *(entry["translated_body"] for entry in manifest["entries"]),
    ])
    table = "\n".join(
        f"    {{0x{entry['address']}u, __imp__sub_{entry['address']}}},"
        for entry in manifest["entries"])
    template = (ROOT / "LostOdysseyRecompSemantics/tests"
                / "string_property_initializer_oracle.cpp").read_text(encoding="utf-8")
    if template.count("/* ENTRY_TABLE */") != 1:
        raise ValueError("string-property oracle table marker changed")
    sources = [
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
    compile_and_run("string-property", original.encode("utf-8"),
                    template.replace("/* ENTRY_TABLE */", table), sources,
                    args.output)


if __name__ == "__main__":
    main()
