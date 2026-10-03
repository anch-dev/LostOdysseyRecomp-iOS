"""Pin two complete descriptor dispatch/release PPC bodies at fixed lines."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SPECS = (
    ("8240EFC0", "ppc_recomp.16.cpp", 13325, 32,
     ["82B7A6E0"], [], 1, "b 0x82b7a730",
     {"r3": "full object containing node chain at +124",
      "r4-r8": "full offset/additional/limit/flags/extra inputs",
      "result": "full last dynamic r3, or full initial r3 when empty"}),
    ("82401C58", "ppc_recomp.15.cpp", 33776, 24,
     ["82B7A6EC", "823F3340", "82507598", "823F3340",
      "82507598", "823F3340"],
     ["823F3340", "82507598", "823F3340", "82507598",
      "823F3340"], 0, "b 0x82b7a73c",
     {"r3": "full object containing arrays at +52 and +32",
      "result": "full live r31 after last child"}),
)


def originals(ppc_root: Path) -> list[dict]:
    entries = []
    for address, filename, line, count, branches, calls, indirect, tail, parameters in SPECS:
        path = f"LostOdysseyRecompLib/ppc/{filename}"
        body = extract_originals([{"address": address,
            "generated_ppc_path": path, "line": line}], ppc_root).decode("utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        actual_branches = [target.upper() for target in re.findall(
            r"^\s*// bl 0x([0-9a-fA-F]{8})$", body, re.M)]
        direct = re.findall(r"\bsub_([0-9A-F]{8})\(ctx, base\);", body)
        if len(instructions) != count or actual_branches != branches or \
                direct != calls or body.count("PPC_CALL_INDIRECT_FUNC(") != indirect or \
                instructions[0] != "mflr r12" or instructions[-1] != tail or \
                ("stwu r1,-144(r1)" if address == "8240EFC0" else
                 "stwu r1,-112(r1)") not in instructions:
            raise ValueError(f"{address} complete PPC body or calls changed")
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
    return {"schema_version": 1, "family": "metadata_descriptor_lifecycle",
        "entry_count": len(SPECS), "entries": originals(ppc_root),
        "limitations": [
            "82401C58 composes already accepted 823F3340 and 82507598 algorithms; they add no credit.",
            "The vtable+360 target and manager release/resize methods remain explicit dynamic services.",
            "Generic lower ABI/frame effects outside exposed interfaces, MMIO, and nonterminating node lists are excluded.",
        ]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_descriptor_lifecycle_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = generate(args.ppc_root)
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("descriptor-lifecycle full-body manifest changed")
    print("metadata-descriptor-lifecycle: 2 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
