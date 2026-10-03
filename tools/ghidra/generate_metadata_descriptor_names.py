"""Pin two complete descriptor-name PPC functions at fixed primary lines."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SPECS = (
    ("822A9668", "ppc_recomp.1.cpp", 19764, 55,
     ["82B7A6EC", "8232CED8", "8229C8B0", "8232D418",
      "8232D418", "82298938", "82298938", "82298938", "8229C8B0"],
     {"r3": "full destination header", "r4": "full packed-name pair",
      "result": "full destination r3"}),
    ("823AC8E0", "ppc_recomp.11.cpp", 26574, 69,
     ["82B7A6E8", "823AC8E0", "8232D418", "8232D378",
      "82298938", "82298938", "8229C8B0", "822A9668",
      "8232D378", "82298938", "8229F5E0"],
     {"r3": "full destination header", "r4": "full owner object",
      "r5": "full stop owner", "result": "full destination r3"}),
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
                instructions[-1] not in ("b 0x82b7a73c", "b 0x82b7a738"):
            raise ValueError(f"{address} full body or direct calls changed")
        entries.append({"address": address, "generated_ppc_path": path,
            "line": line, "instruction_count": count,
            "instruction_sequence": instructions, "translated_body": body,
            "direct_calls": calls, "indirect_calls": [],
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-F]+):", body, re.M),
                    "branches": [value for value in instructions if
                                 re.match(r"^b[a-z]*(?:\s|$)", value)]},
            "parameters": {**parameters,
                "caller_sp": "full entry stack pointer"}})
    return entries


def generate(ppc_root: Path) -> dict:
    return {"schema_version": 1, "family": "metadata_descriptor_names",
        "entry_count": len(SPECS), "entries": originals(ppc_root),
        "limitations": [
            "Already accepted direct callees are composed through their actual models; they add no entry credit.",
            "UTF-16 manager vtable targets remain explicit VirtualServices callbacks.",
            "Generic lower ABI/volatile effects outside exposed APIs, MMIO, and unbounded cyclic owner chains are excluded.",
        ]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_descriptor_names_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = generate(args.ppc_root)
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("descriptor-names full-body manifest changed")
    print("metadata-descriptor-names: 2 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
