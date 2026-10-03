"""Validate four complete PPC bodies for bucket tables and indexed upsert."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SOURCE = "LostOdysseyRecompLib/ppc/ppc_recomp.5.cpp"
SPECS = (
    ("823266A0", 48992, 63, ["823267A0", "82326890"], 28),
    ("823267A0", 49139, 59, ["823F3340", "82486C88"], 28),
    ("82326890", 49275, 57, ["822C42D8", "823267A0"], 28),
    ("82326B08", 49642, 59, ["823F3340", "82486C88"], 16),
)


def originals(ppc_root: Path) -> list[dict]:
    result = []
    for address, line, count, calls, stride in SPECS:
        source = {"address": address, "generated_ppc_path": SOURCE,
                  "line": line}
        body = extract_originals([source], ppc_root).decode("utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        direct = [call.upper() for call in
                  re.findall(r"^\s*// bl 0x([0-9a-f]{8})$", body, re.M)
                  if call.upper() not in ("82B7A6E8", "82B7A6EC")]
        if len(instructions) != count or direct != calls or \
                "PPC_CALL_INDIRECT_FUNC" in body:
            raise ValueError(f"whole-body shape/dependencies changed: {address}")
        if address in ("823267A0", "82326B08"):
            if instructions[:12] != [
                    "mflr r12", "stw r12,-8(r1)", "std r31,-16(r1)",
                    "stwu r1,-112(r1)", "mr r31,r3", "lwz r3,12(r31)",
                    "bl 0x823f3340", "lis r10,16383", "lwz r11,16(r31)",
                    "ori r10,r10,65535", "rlwinm r3,r11,2,0,29",
                    "cmplw cr6,r11,r10"] or \
                    f"addi r10,r10,{stride}" not in instructions or \
                    "std r5,80(r1)" not in instructions or \
                    "lwz r5,80(r1)" not in instructions:
                raise ValueError(f"rebuild algorithm changed: {address}")
        elif address == "82326890" and not (instructions[:12] == [
                "mflr r12", "bl 0x82b7a6ec", "stwu r1,-128(r1)",
                "mr r29,r4", "std r5,160(r1)", "std r6,168(r1)",
                "li r5,28", "li r6,8", "li r4,1", "mr r31,r3",
                "bl 0x822c42d8", "lwz r10,0(r31)"] and
                instructions[-7:] == [
                    "rlwinm r11,r11,1,0,30", "mr r3,r31",
                    "stw r11,16(r31)", "bl 0x823267a0",
                    "addi r3,r30,12", "addi r1,r1,128",
                    "b 0x82b7a73c"]):
            raise ValueError("append/frame parameters changed")
        elif address == "823266A0" and not (
                instructions[:14] == [
                    "mflr r12", "bl 0x82b7a6e8", "stwu r1,-128(r1)",
                    "mr r31,r3", "mr r30,r4", "mr r29,r5", "mr r28,r6",
                    "lwz r11,12(r31)", "std r30,152(r1)",
                    "std r29,160(r1)", "cmplwi cr6,r11,0",
                    "std r28,168(r1)", "bne cr6,0x823266d8",
                    "bl 0x823267a0"] and
                "bl 0x82326890" in instructions and
                "lwz r9,152(r1)" in instructions and
                "lwz r7,156(r1)" in instructions and
                instructions[-3:] == ["addi r3,r11,12", "addi r1,r1,128",
                                      "b 0x82b7a738"]):
            raise ValueError("upsert key/stack/call shape changed")
        result.append({**source, "instruction_count": count,
                       "direct_calls": calls, "stride": stride,
                       "instruction_sequence": instructions,
                       "translated_body": body})
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/manager_index_tables_families.json")
    parser.add_argument("--write", action="store_true",
        help="write initial exact-body snapshot; normal use validates it")
    args = parser.parse_args()
    expected = {"schema_version": 1, "family": "manager_index_tables",
                "entry_count": 4, "entries": originals(args.ppc_root),
                "limitations": [
                    "Existing manager/array lower APIs bound generic callback register redirection.",
                    "Ordinary mapped RAM width/order is modeled; MMIO/fault width and unbounded concurrent mutation are excluded."]}
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("manager-index full PPC body/parameters changed")
    print("manager-index tables: 4 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
