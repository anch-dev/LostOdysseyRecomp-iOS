"""Strict two-entry shared-metadata admission from exact PPC source positions.

The checked manifest freezes complete translated bodies, instruction order,
CFG labels and branches, direct callees, parameters, and save28/rest28 bodies.
No whole-PPC scan, ignored inventory, or hash is needed for verification.
"""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "LostOdysseyRecompSemantics/instance_shared_metadata_initializer_families.json"
SOURCES = {
    "824108C8": ("ppc_recomp.16.cpp", 17135),
    "82412100": ("ppc_recomp.16.cpp", 20797),
}
HELPERS = {"__savegprlr_28": 5671, "__restgprlr_28": 6257}
CALLEES = {"824108C8": ["82403148", "82403200"],
           "82412100": ["824108C8"]}
PARAMETERS = {
    "824108C8": {"kind": "shared_metadata", "frame_size": 160,
                 "save_helper": "__savegprlr_28",
                 "restore_helper": "__restgprlr_28",
                 "vtable": "0x82005160", "global": "0x83315F60",
                 "constructor": "82403148", "registration": "82403200",
                 "constructor_argument_full": "0xFFFFFFFF8218C210",
                 "initial_r28": 8, "initial_r30": 0,
                 "outgoing_word_offsets": [80, 84, 88, 92, 96, 100, 104, 108]},
    "82412100": {"kind": "lowword_null_tail", "tail": "824108C8"},
}


def extract(source_root, filename, line, symbol):
    path = source_root / "LostOdysseyRecompLib/ppc" / filename
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    start = line - 1
    if lines[start].rstrip() != f"PPC_FUNC_IMPL(__imp__{symbol}) {{":
        raise ValueError(f"fixed source position changed: {symbol}")
    end = next((index for index in range(start + 1,
                min(start + 450, len(lines))) if lines[index].rstrip() == "}"), None)
    if end is None:
        raise ValueError(f"incomplete PPC body: {symbol}")
    body = "".join(lines[start:end + 1])
    instructions = [instruction.strip() for instruction in re.findall(
        r"^\s*// (.+)$", body, re.MULTILINE)]
    if not instructions:
        raise ValueError(f"empty PPC instruction list: {symbol}")
    return {"source": f"LostOdysseyRecompLib/ppc/{filename}",
            "source_line": line, "translated_body": body,
            "instructions": instructions,
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-Fa-f]+):",
                                         body, re.MULTILINE),
                    "branches": [instruction for instruction in instructions
                                 if re.match(r"(?:b|bne|beqlr|blr)\b",
                                             instruction)]},
            "callees": [target.upper() for target in re.findall(
                r"\bsub_([0-9A-Fa-f]{8})\(ctx, base\);", body)]}


def reviewed(source_root):
    entries = []
    for address, (filename, line) in SOURCES.items():
        entry = extract(source_root, filename, line, f"sub_{address}")
        if entry["callees"] != CALLEES[address]:
            raise ValueError(f"direct callee set changed: {address}")
        if address == "824108C8" and len(entry["instructions"]) != 88:
            raise ValueError("shared metadata instruction count changed")
        if address == "82412100" and len(entry["instructions"]) != 3:
            raise ValueError("null tail instruction count changed")
        entries.append({"address": address,
                        "parameters": PARAMETERS[address], **entry})
    helpers = []
    for name, line in HELPERS.items():
        entry = extract(source_root, "ppc_recomp.175.cpp", line, name)
        if entry["callees"]:
            raise ValueError(f"ABI helper acquired a direct callee: {name}")
        helpers.append({"name": name, **entry})
    return {"schema_version": 1,
            "family": "instance_shared_metadata_initializer",
            "entry_count": 2, "entries": entries,
            "abi_helper_count": 2, "abi_helpers": helpers}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--write-reviewed-baseline", action="store_true")
    args = parser.parse_args()
    rendered = json.dumps(reviewed(args.source_root), indent=2) + "\n"
    if args.write_reviewed_baseline:
        OUTPUT.write_text(rendered, encoding="utf-8", newline="\n")
    elif not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != rendered:
        raise ValueError("checked shared metadata baseline differs from PPC")
    print("Verified 2 complete PPC bodies and 2 exact ABI helper bodies")


if __name__ == "__main__":
    main()
