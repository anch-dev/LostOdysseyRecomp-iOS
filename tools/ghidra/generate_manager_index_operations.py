"""Admit two exact 16-byte manager-index upsert PPC bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SPECS = (
    ("82326978", "LostOdysseyRecompLib/ppc/ppc_recomp.5.cpp", 49407, 100,
     ["82326B08", "8229F678", "82326B08"], "single"),
    ("82713CF8", "LostOdysseyRecompLib/ppc/ppc_recomp.66.cpp", 29969, 97,
     ["82326B08", "8229F678", "82326B08"], "word"),
)


def originals(ppc_root: Path) -> list[dict]:
    result = []
    for address, source_path, line, count, calls, value_kind in SPECS:
        entry = {"address": address, "generated_ppc_path": source_path,
                 "line": line}
        body = extract_originals([entry], ppc_root).decode("utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        direct = [call.upper() for call in re.findall(
            r"^\s*// bl 0x([0-9a-f]{8})$", body, re.M)
            if call.upper() not in ("82B7A6E8", "82B7A6EC")]
        if len(instructions) != count or direct != calls or \
                "PPC_CALL_INDIRECT_FUNC" in body:
            raise ValueError(f"index-operation body/dependencies changed: {address}")
        if value_kind == "single":
            if instructions[:14] != [
                    "mflr r12", "bl 0x82b7a6ec", "stfd f31,-40(r1)",
                    "stwu r1,-128(r1)", "mr r31,r3", "fmr f31,f1",
                    "mr r29,r4", "lwz r11,12(r31)", "std r29,152(r1)",
                    "cmplwi cr6,r11,0", "bne cr6,0x823269a8",
                    "bl 0x82326b08", "lwz r30,4(r31)",
                    "cmpwi cr6,r30,0"] or \
                    instructions.count("stfs f31,12(r11)") != 1 or \
                    instructions.count("stfs f31,12(r10)") != 1 or \
                    instructions.count("lfd f31,-40(r1)") != 2:
                raise ValueError("single-value FP frame/store shape changed")
        else:
            if instructions[:13] != [
                    "mflr r12", "bl 0x82b7a6e8", "stwu r1,-128(r1)",
                    "mr r31,r3", "mr r28,r4", "mr r29,r5",
                    "lwz r11,12(r31)", "std r28,152(r1)",
                    "cmplwi cr6,r11,0", "bne cr6,0x82713d24",
                    "bl 0x82326b08", "lwz r30,4(r31)",
                    "cmpwi cr6,r30,0"] or \
                    "std r28,4(r11)" not in instructions or \
                    "stw r29,12(r11)" not in instructions or \
                    "stw r29,12(r10)" not in instructions:
                raise ValueError("word-value frame/store shape changed")
        result.append({**entry, "instruction_count": count,
                       "direct_calls": calls, "value_kind": value_kind,
                       "instruction_sequence": instructions,
                       "translated_body": body})
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/manager_index_operations_families.json")
    parser.add_argument("--write", action="store_true",
        help="write initial exact-body snapshot; normal use validates it")
    args = parser.parse_args()
    expected = {"schema_version": 1, "family": "manager_index_operations",
                "entry_count": 2, "entries": originals(args.ppc_root),
                "limitations": [
                    "Existing manager-index and ResizeArray lower APIs bound generic callback register redirection.",
                    "Finite generated-C++ FP conversion is compared; signaling NaN and hardware FPSCR edge behavior are not proven.",
                    "Ordinary RAM width/order is modeled; MMIO/fault width is excluded."]}
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("manager-index operations full PPC body changed")
    print("manager-index operations: 2 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
