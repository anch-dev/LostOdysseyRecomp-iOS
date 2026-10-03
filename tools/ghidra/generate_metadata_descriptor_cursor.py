"""Pin the two complete descriptor cursor PPC bodies at primary source lines."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SPECS = (
    ("822A6EF8", "ppc_recomp.1.cpp", 13642, 40, [], 1,
     {"r3": "full cursor header", "r4": "full preserved caller argument",
      "result": "full residual r3, including virtual return"}),
    ("82406568", "ppc_recomp.15.cpp", 44742, 20,
     ["822A6EF8"], 0,
     {"r3": "full cursor header", "r4": "full initial node",
      "result": "full live r31 after the child restores"}),
)


def originals(ppc_root: Path) -> list[dict]:
    entries = []
    for address, filename, line, count, calls, indirect, parameters in SPECS:
        path = f"LostOdysseyRecompLib/ppc/{filename}"
        body = extract_originals([{"address": address,
            "generated_ppc_path": path, "line": line}], ppc_root).decode("utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        actual_calls = [target.upper() for target in re.findall(
            r"^\s*// bl 0x([0-9a-fA-F]{8})$", body, re.M)]
        if len(instructions) != count or actual_calls != calls or \
                body.count("PPC_CALL_INDIRECT_FUNC(") != indirect or \
                instructions[0] != "mflr r12" or instructions[-1] != "blr" or \
                "std r31,-16(r1)" not in instructions or \
                "stwu r1,-96(r1)" not in instructions:
            raise ValueError(f"{address} whole body or call sequence changed")
        entries.append({"address": address, "generated_ppc_path": path,
            "line": line, "instruction_count": count,
            "instruction_sequence": instructions, "translated_body": body,
            "direct_calls": calls, "indirect_calls": indirect,
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-F]+):", body, re.M),
                    "branches": [value for value in instructions if
                                 re.match(r"^b[a-z]*(?:\s|$)", value)]},
            "parameters": {**parameters,
                "caller_sp": "full entry guest stack pointer"}})
    return entries


def generate(ppc_root: Path) -> dict:
    return {"schema_version": 1, "family": "metadata_descriptor_cursor",
        "entry_count": len(SPECS), "entries": originals(ppc_root),
        "limitations": [
            "82406568 composes the actual recovered 822A6EF8 body; no known direct target is opaque.",
            "The node vtable+284 target is an explicit dynamic callback, not a recovered target implementation.",
            "Other volatile ABI state, MMIO/faults, and nonterminating guest lists are excluded.",
        ]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_descriptor_cursor_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = generate(args.ppc_root)
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("descriptor-cursor full-body manifest changed")
    print("metadata-descriptor-cursor: 2 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
