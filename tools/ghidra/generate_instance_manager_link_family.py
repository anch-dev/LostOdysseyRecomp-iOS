"""Admit the two exact PPC bodies in the manager-link foundation family.

This intentionally rejects the still-open 827010D0/82700D10 chain. The
manifest retains full translated bodies to catch non-instruction source drift.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SOURCES = (
    ("82700F28", 10255, 28, ["82700FD8"]),
    ("82700FD8", 10369, 34, []),
)
SOURCE_PATH = "LostOdysseyRecompLib/ppc/ppc_recomp.65.cpp"


def originals(ppc_root: Path) -> list[dict]:
    entries = []
    for address, line, count, calls in SOURCES:
        source = {"address": address, "line": line,
                  "generated_ppc_path": SOURCE_PATH}
        body = extract_originals([source], ppc_root).decode("utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        actual_calls = [value.upper() for value in
                        re.findall(r"^\s*// bl 0x([0-9a-f]{8})$", body, re.M)]
        if len(instructions) != count or actual_calls != calls or \
                body.count("PPC_CALL_INDIRECT_FUNC") or \
                re.search(r"^\s*// b(?:ne|eq|gt|lt|le|ge|ctr|ctrl)\b", body, re.M):
            raise ValueError(f"unexpected PPC instruction/CFG/call shape: {address}")
        if address == "82700F28" and not (
                instructions[13:20] == [
                    "addi r3,r31,16", "stw r11,4(r31)", "stw r10,0(r31)",
                    "stw r9,4(r31)", "stw r4,8(r31)", "stw r30,12(r31)",
                    "bl 0x82700fd8"] and
                instructions[-8:] == [
                    "mr r3,r31", "stw r30,144(r31)", "addi r1,r1,112",
                    "lwz r12,-8(r1)", "mtlr r12", "ld r30,-24(r1)",
                    "ld r31,-16(r1)", "blr"]):
            raise ValueError("82700F28 owner parameter/order changed")
        if address == "82700FD8" and not (
                instructions[:7] == [
                    "lis r11,-32256", "li r10,8", "lfs f0,3664(r11)",
                    "lis r11,-32231", "stw r10,24(r3)",
                    "lfs f13,-27252(r11)", "li r11,0"] and
                instructions[-13:] == [
                    "stfs f0,80(r3)", "stfs f0,84(r3)", "stfs f0,88(r3)",
                    "stfs f13,92(r3)", "stfs f0,96(r3)",
                    "stfs f0,100(r3)", "stfs f0,104(r3)",
                    "stfs f13,108(r3)", "stfs f0,112(r3)",
                    "stfs f0,116(r3)", "stfs f0,120(r3)",
                    "stfs f13,124(r3)", "blr"]):
            raise ValueError("82700FD8 data layout changed")
        entries.append({**source, "instruction_count": count,
                        "direct_calls": calls,
                        "instruction_sequence": instructions,
                        "translated_body": body})
    return entries


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path,
        default=Path.home() / "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/instance_manager_link_families.json")
    parser.add_argument("--write", action="store_true",
        help="write the initial exact-body manifest; normal use validates it")
    args = parser.parse_args()
    actual = {"schema_version": 1, "family": "instance_manager_link_foundations",
              "entry_count": 2, "entries": originals(args.ppc_root),
              "excluded_open_chain": ["827010D0", "82700D10"],
              "limitations": [
                  "Generated C++ lfs/stfs differs for signaling NaN; LoadedSingle uses ISA bit movement.",
                  "Ordinary RAM word stores and frame saves do not model fault/MMIO access width."]}
    if args.write:
        args.manifest.write_text(json.dumps(actual, indent=2) + "\n", encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != actual:
        raise ValueError("manager-link original full bodies or parameters changed")
    print("instance manager-link foundations: 2 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
