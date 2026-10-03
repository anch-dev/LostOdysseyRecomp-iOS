"""Pin four complete descriptor-index PPC bodies and their dependencies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SPECS = (
    ("82523C48", "ppc_recomp.34.cpp", 26060, 62,
     ["823F3340", "82486C88"],
     {"r3": "full descriptor-index table", "result": "full allocation r3"}),
    ("8256B910", "ppc_recomp.37.cpp", 32662, 52,
     ["82B7A6E8", "822C42D8", "82523C48"],
     {"r3": "full table", "r4": "full key pointer",
      "r5": "full value word", "result": "full slot-plus-eight r3"}),
    ("826BD860", "ppc_recomp.61.cpp", 11257, 53,
     ["82B7A6EC", "82523C48", "8256B910"],
     {"r3": "full table", "r4": "full key pointer",
      "r5": "full value word", "result": "hit slot or appended full r3"}),
    ("82408D28", "ppc_recomp.15.cpp", 50810, 39,
     ["826BD860"],
     {"r3": "full destination", "r4": "full owner",
      "r5": "full optional key pointer", "result": "nested full r3"}),
)


def originals(ppc_root: Path) -> list[dict]:
    entries = []
    for address, filename, line, count, calls, parameters in SPECS:
        path = f"LostOdysseyRecompLib/ppc/{filename}"
        body = extract_originals([{"address": address,
            "generated_ppc_path": path, "line": line}], ppc_root).decode("utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        actual_calls = [target.upper() for target in re.findall(
            r"^\s*// bl 0x([0-9a-fA-F]{8})$", body, re.M)]
        if len(instructions) != count or actual_calls != calls or \
                "PPC_CALL_INDIRECT_FUNC" in body or \
                instructions[0] != "mflr r12" or \
                instructions[-1] not in ("blr", "b 0x82b7a738",
                                       "b 0x82b7a73c"):
            raise ValueError(f"{address} full body or call sequence changed")
        entries.append({"address": address, "generated_ppc_path": path,
            "line": line, "instruction_count": count,
            "instruction_sequence": instructions,
            "translated_body": body, "direct_calls": calls,
            "indirect_calls": [],
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-F]+):", body, re.M),
                    "branches": [value for value in instructions if
                                 re.match(r"^b[a-z]*(?:\s|$)", value)]},
            "parameters": {**parameters,
                "caller_sp": "full entry stack pointer"}})
    return entries


def generate(ppc_root: Path) -> dict:
    return {"schema_version": 1, "family": "metadata_descriptor_index",
        "entry_count": len(SPECS), "entries": originals(ppc_root),
        "limitations": [
            "823F3340, 82486C88, and 822C42D8 compose their already accepted models; they add no entry credit here.",
            "Manager allocation/release/resize methods remain explicit dynamic services in those lower models.",
            "Existing lower APIs do not expose all generic volatile/nonvolatile register and frame effects.",
            "Ordinary mapped guest RAM and bounded callbacks are compared; MMIO faults and concurrent mutation are excluded.",
        ]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_descriptor_index_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = generate(args.ppc_root)
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("descriptor-index full-body manifest changed")
    print("metadata-descriptor-index: 4 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
