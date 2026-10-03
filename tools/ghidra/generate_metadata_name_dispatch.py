"""Pin the complete 825E7600 metadata-name dispatch PPC body."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


ADDRESS = "825E7600"
SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.46.cpp"
LINE = 9576
CALLS = ["82B7A6EC", "82296D30"]


def original(ppc_root: Path) -> dict:
    body = extract_originals([{"address": ADDRESS,
        "generated_ppc_path": SOURCE, "line": LINE}], ppc_root).decode("utf-8")
    instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
    calls = [value.upper() for value in re.findall(
        r"^\s*// bl 0x([0-9a-fA-F]{8})$", body, re.M)]
    if len(instructions) != 37 or calls != CALLS or \
            body.count("PPC_CALL_INDIRECT_FUNC") != 1 or \
            instructions[:3] != ["mflr r12", "bl 0x82b7a6ec",
                                 "stwu r1,-112(r1)"] or \
            instructions[-7:] != ["lwz r11,0(r3)", "lwz r11,264(r11)",
                                   "mtctr r11", "bctrl", "mr r3,r31",
                                   "addi r1,r1,112", "b 0x82b7a73c"]:
        raise ValueError("825E7600 complete body/calls changed")
    return {"address": ADDRESS, "generated_ppc_path": SOURCE, "line": LINE,
            "instruction_count": 37,
            "instruction_sequence": instructions,
            "translated_body": body,
            "direct_calls": CALLS,
            "indirect_calls": [{"kind": "vtable+264",
                                "return_address": "825E7688",
                                "target_mask": "~3"}],
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-F]+):", body, re.M),
                    "branches": [value for value in instructions if
                                 re.match(r"^b[a-z]*(?:\s|$)", value)]},
            "parameters": {"r3": "full object pointer",
                           "r5": "full word stored at object+100",
                           "r6": "full lookup argument r4",
                           "r7": "full 64-bit value stored at object+76",
                           "caller_sp": "full entry stack pointer",
                           "unused_input": "r4"}}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_name_dispatch_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = {"schema_version": 1, "family": "metadata_name_dispatch",
                "entry_count": 1, "entries": [original(args.ppc_root)],
                "limitations": [
                    "82296D30 is composed through its accepted bounded model, not a direct-call stand-in.",
                    "The vtable+264 target is an explicit dynamic service; its implementation is not recovered.",
                    "Only explicit post-lookup full r3/r4, SP and LR inputs are guaranteed to that service; generic lower volatile r5-r7 effects are not exposed by the lookup API.",
                    "Ordinary guest RAM and own known frame aliases are modeled; lower generic ABI and hardware/MMIO faults are excluded.",
                ]}
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("825E7600 full-body manifest changed")
    print("metadata-name dispatch: 1 exact PPC body accepted")


if __name__ == "__main__":
    main()
