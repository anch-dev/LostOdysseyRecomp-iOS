"""Pin the exact 39-instruction ring reservation, retaining its lwsync boundary."""

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

OUTPUT = ROOT / "LostOdysseyRecompSemantics/ring_reservation_families.json"
ENTRY = {"address": "82290AB8",
         "generated_ppc_path": "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp",
         "line": 1653}


def reviewed(ppc_root):
    body = extract_originals([ENTRY], ppc_root).decode().replace("\r\n", "\n")
    instructions = [x.strip() for x in re.findall(r"^\s*// (.+)$", body, re.M)]
    expected = [
        "stw r4,0(r3)", "li r9,1", "lwz r11,24(r4)", "add r10,r11,r5",
        "addi r11,r11,-1", "addi r10,r10,-1", "andc r11,r10,r11",
        "stw r11,8(r3)", "stw r9,16(r4)", "lwz r11,0(r3)",
        "lwz r8,20(r11)", "lwz r10,8(r11)", "cmplw cr6,r8,r10",
        "ble cr6,0x82290b04", "lwz r10,8(r11)", "lwz r9,8(r3)",
        "add r10,r10,r9", "cmplw cr6,r10,r8", "bge cr6,0x82290adc",
        "lwz r10,8(r11)", "lwz r9,8(r3)", "lwz r7,4(r11)",
        "add r10,r10,r9", "cmplw cr6,r10,r7", "ble cr6,0x82290b44",
        "lwz r10,0(r11)", "cmplw cr6,r8,r10", "beq cr6,0x82290adc",
        "lwz r10,8(r11)", "stw r10,12(r11)", "lwsync", "lwz r11,0(r3)",
        "lwz r10,0(r11)", "stw r10,8(r11)", "b 0x82290adc",
        "lwz r11,0(r3)", "lwz r11,8(r11)", "stw r11,4(r3)", "blr",
    ]
    if instructions != expected or "PPC_CALL" in body or "sub_" in body.split("{", 1)[1]:
        raise ValueError("ring reservation instruction/call structure changed")
    entry = {**ENTRY, "instruction_sequence": instructions, "translated_body": body,
             "cfg": {"labels": re.findall(r"^(loc_[0-9A-Fa-f]+):", body, re.M),
                     "branches": [x for x in instructions if x.startswith("b")]},
             "parameters": {"descriptor_words": [0, 4, 8],
                "ring_fields": {"base": 0, "end": 4, "producer": 8,
                                "wrap_limit": 12, "reserved": 16,
                                "consumer": 20, "alignment": 24},
                "synchronization": "lwsync after wrap-limit store",
                "wait": "original live-field loops, no timeout"}}
    return {"schema_version": 1, "family": "ring_reservation",
            "entry_count": 1, "entries": [entry],
            "validation_limit": "Generated PPC omits lwsync. Ordinary RAM finite paths do not prove concurrency or hardware synchronization."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--write-reviewed-baseline", action="store_true")
    args = parser.parse_args()
    rendered = json.dumps(reviewed(args.source_root / "LostOdysseyRecompLib/ppc"), indent=2) + "\n"
    if args.write_reviewed_baseline:
        OUTPUT.write_text(rendered, encoding="utf-8", newline="\n")
    elif OUTPUT.read_text(encoding="utf-8") != rendered:
        raise ValueError("reviewed ring reservation differs from PPC")
    print("Verified exact ring reservation body, 39 instructions and lwsync boundary")


if __name__ == "__main__":
    main()
