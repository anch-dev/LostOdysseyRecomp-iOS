"""Pin the complete 823F3298 manager vtable+4 dispatch PPC body."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


ADDRESS = "823F3298"
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.14.cpp"
LINE = 30568
CALLS = ["82B7A6EC", "827C5F38"]


def original(ppc_root: Path) -> dict:
    body = extract_originals([{"address": ADDRESS,
        "generated_ppc_path": SOURCE, "line": LINE}], ppc_root).decode("utf-8")
    instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
    calls = [value.upper() for value in re.findall(
        r"^\s*// bl 0x([0-9a-fA-F]{8})$", body, re.M)]
    labels = re.findall(r"^(loc_[0-9A-F]+):", body, re.M)
    if len(instructions) != 19 or calls != CALLS or \
            body.count("PPC_CALL_INDIRECT_FUNC") != 1 or \
            labels != ["loc_823F32C4"] or \
            instructions[:3] != ["mflr r12", "bl 0x82b7a6ec",
                                 "stwu r1,-112(r1)"] or \
            instructions[-3:] != ["bctrl", "addi r1,r1,112",
                                   "b 0x82b7a73c"]:
        raise ValueError("823F3298 full PPC body/calls changed")
    return {"address": ADDRESS, "generated_ppc_path": SOURCE,
        "line": LINE, "instruction_count": 19,
        "instruction_sequence": instructions,
        "translated_body": body, "direct_calls": CALLS,
        "indirect_calls": [{"kind": "manager vtable+4",
            "return_address": "823F32DC", "target_mask": "~3"}],
        "cfg": {"labels": labels, "branches": [value for value in
            instructions if re.match(r"^b[a-z]*(?:\s|$)", value)]},
        "parameters": {"r3": "full argument passed as dynamic r4",
            "r4": "full argument passed as dynamic r5",
            "caller_sp": "full entry SP", "result": "dynamic full r3"}}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_manager_dispatch_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = {"schema_version": 1, "family": "metadata_manager_dispatch",
        "entry_count": 1, "entries": [original(args.ppc_root)],
        "limitations": [
            "827C5F38 is composed through its accepted InitializeManager model, not an opaque direct-call stand-in.",
            "Manager vtable+4 is an explicit dynamic service; its implementation and semantic purpose are not recovered.",
            "The existing InitializeManager API does not expose all lower generic register/frame effects.",
            "Ordinary mapped guest RAM is covered; MMIO faults and concurrent manager mutation are excluded.",
        ]}
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("manager-dispatch full-body manifest changed")
    print("metadata-manager-dispatch: 1 exact PPC body accepted")


if __name__ == "__main__":
    main()
