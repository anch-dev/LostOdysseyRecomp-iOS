"""Freeze five connection/bit-writer PPC bodies and their ABI save helpers.

Reads only reviewed source positions. The checked manifest retains complete
translated bodies, instruction sequences, CFG branches, calls and parameters;
normal verification cannot silently update that baseline.
"""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "LostOdysseyRecompSemantics/instance_connection_initializer_families.json"
SOURCES = {
    "82679F50": ("ppc_recomp.56.cpp", 25239),
    "8267A040": ("ppc_recomp.56.cpp", 25415),
    "8267A1F0": ("ppc_recomp.56.cpp", 25725),
    "8272CBD8": ("ppc_recomp.68.cpp", 12083),
    "82752768": ("ppc_recomp.70.cpp", 1424),
}
HELPERS = {
    "__savegprlr_25": 5599,
    "__savegprlr_29": 5691,
    "__restgprlr_25": 6179,
    "__restgprlr_29": 6279,
}
CALLEES = {
    "82679F50": ["8267A1F0"],
    "8267A040": ["8267A1F0"],
    "8267A1F0": ["82496948", "82752768", "82752768"],
    "8272CBD8": ["8267A1F0", "82B7BC40"],
    "82752768": ["823F34B8", "8229F678", "82B7BC40"],
}
PARAMETERS = {
    "82679F50": {"kind": "null_tail", "tail": "8267A1F0"},
    "8267A040": {"kind": "child", "frame_size": 96,
                 "vtables": ["0x821F6F20", "0x821F6C9C", "0x821F7064"]},
    "8267A1F0": {"kind": "net", "frame_size": 176,
                 "save_helper": "__savegprlr_25",
                 "restore_helper": "__restgprlr_25",
                 "first_fp_source": "0x82000FE8",
                 "second_fp_source": "0x8218958C",
                 "third_fp_source": "0x822184DC",
                 "property": "82496948", "bit_writer": "82752768"},
    "8272CBD8": {"kind": "tcpip", "frame_size": 112,
                 "vtables": ["0x82210890", "0x821F6C9C", "0x822109D4"],
                 "fill_offset": 20320, "fill_bytes": 16},
    "82752768": {"kind": "bit_writer", "frame_size": 112,
                 "save_helper": "__savegprlr_29",
                 "restore_helper": "__restgprlr_29",
                 "archive": "823F34B8", "element_size": 1,
                 "resize_argument": 8, "capacity_formula": "signed32(r4.low+7)>>3",
                 "header_write_order": [116, 112, 120],
                 "live_reload_order": [116, 112],
                 "vtable_before_archive": "0x82189968",
                 "vtable_after_archive": "0x821FE828"},
}


def extract(source_root, filename, line, symbol):
    path = source_root / "LostOdysseyRecompLib/ppc" / filename
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    start = line - 1
    marker = f"PPC_FUNC_IMPL(__imp__{symbol}) {{"
    if lines[start].rstrip() != marker:
        raise ValueError(f"fixed PPC source position changed: {symbol}")
    end = next((index for index in range(start + 1,
                min(start + 700, len(lines))) if lines[index].rstrip() == "}"), None)
    if end is None:
        raise ValueError(f"PPC body incomplete: {symbol}")
    body = "".join(lines[start:end + 1])
    instructions = [instruction.strip() for instruction in re.findall(
        r"^\s*// (.+)$", body, re.MULTILINE)]
    if not instructions:
        raise ValueError(f"empty instruction list: {symbol}")
    calls = [target.upper() for target in re.findall(
        r"\bsub_([0-9A-Fa-f]{8})\(ctx, base\);", body)]
    return {"source": f"LostOdysseyRecompLib/ppc/{filename}",
            "source_line": line, "translated_body": body,
            "instructions": instructions,
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-Fa-f]+):",
                                         body, re.MULTILINE),
                    "branches": [instruction for instruction in instructions
                                 if re.match(r"(?:b|beq|bne|beqlr|blr)\b",
                                             instruction)]},
            "callees": calls}


def reviewed(source_root):
    entries = []
    for address, (filename, line) in SOURCES.items():
        entry = extract(source_root, filename, line, f"sub_{address}")
        if entry["callees"] != CALLEES[address]:
            raise ValueError(f"direct calls changed: {address}")
        entry = {"address": address, "parameters": PARAMETERS[address], **entry}
        entries.append(entry)
    helpers = []
    for name, line in HELPERS.items():
        entry = extract(source_root, "ppc_recomp.175.cpp", line, name)
        if entry["callees"]:
            raise ValueError(f"ABI helper acquired a callee: {name}")
        helpers.append({"name": name, **entry})
    return {"schema_version": 1,
            "family": "instance_connection_initializer",
            "entry_count": 5, "entries": entries,
            "abi_helper_count": 4, "abi_helpers": helpers}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--write-reviewed-baseline", action="store_true")
    args = parser.parse_args()
    rendered = json.dumps(reviewed(args.source_root), indent=2) + "\n"
    if args.write_reviewed_baseline:
        OUTPUT.write_text(rendered, encoding="utf-8", newline="\n")
    elif not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != rendered:
        raise ValueError("checked connection manifest differs from exact PPC")
    print("Verified 5 complete PPC bodies and 4 exact ABI helper bodies")


if __name__ == "__main__":
    main()
