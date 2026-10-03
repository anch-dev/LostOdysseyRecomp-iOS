"""Pin three exact PPC name-fold, hash, and index-insertion bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import ROOT, extract_originals


SPECS = (
    ("82296FE8", "ppc_recomp.0.cpp", 16685, 126, [], "fold_utf16"),
    ("82296F68", "ppc_recomp.0.cpp", 16605, 32,
     ["82B7A6EC", "82296FE8"], "hash_name"),
    ("823F44E8", "ppc_recomp.15.cpp", 2197, 55,
     ["82B7A6E4", "82296F68", "8229F678"], "insert_name"),
)


def originals(ppc_root: Path) -> list[dict]:
    entries = []
    for address, filename, line, count, calls, kind in SPECS:
        source = f"LostOdysseyRecompLib/ppc/{filename}"
        body = extract_originals([{"address": address,
            "generated_ppc_path": source, "line": line}], ppc_root).decode(
                "utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        direct = [value.upper() for value in re.findall(
            r"^\s*// bl 0x([0-9a-fA-F]{8})$", body, re.M)]
        if len(instructions) != count or direct != calls or \
                "PPC_CALL_INDIRECT_FUNC" in body:
            raise ValueError(f"name-index PPC body/calls changed: {address}")
        labels = re.findall(r"^(loc_[0-9A-F]+):", body, re.M)
        branches = [item for item in instructions if re.match(
            r"^(?:b[a-z]*|bdnz)(?:\s|$)", item)]
        if kind == "fold_utf16":
            cases = [(int(number), target) for number, target in re.findall(
                r"case (\d+):\s*goto (loc_[0-9A-F]+);", body)]
            if [number for number, _ in cases] != list(range(100)) or \
                    dict(cases)[0] != "loc_822971A8" or \
                    dict(cases)[99] != "loc_822971A0" or \
                    [n for n, target in cases if target == "loc_822971DC"] != \
                    [52, 67, 84, 91] or \
                    "ctx.r0.u64 = PPC_LOAD_U32" not in body or \
                    "ctx.ctr.u64 = ctx.r0.u64" not in body:
                raise ValueError("case-fold jump-table shape changed")
        elif kind == "hash_name":
            if "PPC_LOAD_U16(ctx.r31.u32 + 0)" not in body or \
                    instructions.count("lwzx r9,r9,r29") != 2 or \
                    "bne cr6,0x82296f90" not in instructions:
                raise ValueError("UTF-16 hash loop shape changed")
        elif kind == "insert_name":
            if instructions.count("stw r30,0(r10)") != 0 or \
                    "stwx r30,r9,r10" not in instructions or \
                    "stwx r30,r11,r10" not in instructions or \
                    "bgt cr6,0x823f45ac" not in instructions or \
                    "ble cr6,0x823f4538" not in instructions:
                raise ValueError("name-index insertion shape changed")
        entries.append({"address": address, "kind": kind,
                        "generated_ppc_path": source, "line": line,
                        "instruction_count": count,
                        "instruction_sequence": instructions,
                        "cfg": {"labels": labels,
                                "branch_instructions": branches},
                        "direct_calls": calls,
                        "parameters": {
                            "fold_utf16": "full r3 UTF-16 unit",
                            "hash_name": "full r3 source pointer, full caller SP",
                            "insert_name": "full r3 record pointer, full caller SP",
                        }[kind],
                        "translated_body": body})
    return entries


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--manifest", type=Path, default=ROOT /
        "LostOdysseyRecompSemantics/metadata_name_index_families.json")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = {"schema_version": 1, "family": "metadata_name_index",
                "entry_count": 3, "entries": originals(args.ppc_root),
                "limitations": [
                    "Guest jump-table and hash-table reads remain live ordinary RAM reads.",
                    "ResizeArray lower API bounds callback return to a 32-bit guest address and excludes generic lower ABI register/frame redirection.",
                    "Selected volatile registers and condition/XER state beyond r0/CTR are outside the public entry ABI.",
                ]}
    if args.write:
        args.manifest.write_text(json.dumps(expected, indent=2) + "\n",
                                 encoding="utf-8")
    elif json.loads(args.manifest.read_text(encoding="utf-8")) != expected:
        raise ValueError("metadata-name index full PPC bodies/CFG changed")
    print("metadata-name index: 3 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
