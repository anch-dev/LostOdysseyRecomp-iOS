"""Pin three complete descriptor-array PPC bodies and their call boundaries."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SPECS = (
    ("823B9268", "ppc_recomp.12.cpp", 11441, 20, ["82298AF8"],
     {"r3": "full mutable array header", "caller_sp": "full entry SP",
      "result": "zero-extended removed last word"}),
    ("822B3F50", "ppc_recomp.1.cpp", 45443, 35,
     ["8229F678", "82B7A0B0"],
     {"r3": "full destination array header",
      "r4": "full source array header", "caller_sp": "full entry SP",
      "result": "full destination r3"}),
    ("82400BC0", "ppc_recomp.15.cpp", 31405, 73,
     ["82B7A6EC", "823B9268", "822C42D8"],
     {"r3": "full descriptor object", "r4": "full requested slot index",
      "caller_sp": "full entry SP", "result": "live full assigned index"}),
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
                instructions[-1] not in ("blr", "b 0x82b7a73c"):
            raise ValueError(f"{address} complete body/call sequence changed")
        labels = re.findall(r"^(loc_[0-9A-F]+):", body, re.M)
        branches = [value for value in instructions if
                    re.match(r"^b[a-z]*(?:\s|$)", value)]
        entries.append({"address": address, "generated_ppc_path": path,
            "line": line, "instruction_count": count,
            "instruction_sequence": instructions,
            "translated_body": body, "direct_calls": calls,
            "indirect_calls": [], "cfg": {"labels": labels,
                "branches": branches}, "parameters": parameters})
    return entries


def generate(ppc_root: Path) -> dict:
    return {"schema_version": 1, "family": "metadata_descriptor_array",
        "entry_count": len(SPECS), "entries": originals(ppc_root),
        "limitations": [
            "The four direct array/copy callees use their already accepted bounded models, not opaque stand-ins.",
            "ArrayResizeServices supplies dynamic manager initialization and resize; their guest implementations are not recovered here.",
            "The existing lower APIs do not expose generic callee volatile registers, nonvolatile redirection, or all lower frame writes.",
            "Ordinary mapped guest RAM is covered; MMIO faults and concurrent global mutation are excluded.",
        ]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_descriptor_array_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = generate(args.ppc_root)
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("descriptor-array full-body manifest changed")
    print("metadata-descriptor-array: 3 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
