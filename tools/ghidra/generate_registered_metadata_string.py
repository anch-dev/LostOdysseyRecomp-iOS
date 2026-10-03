"""Freeze and verify six reviewed string/metadata PPC bodies without a tree scan.

The checked manifest contains each entire translated body, instruction list,
branch labels, direct callees, and differing constants. Normal verification
never reads ignored inventories or rewrites the reviewed baseline.
"""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "LostOdysseyRecompSemantics/registered_metadata_string_families.json"

# Exact source positions came from the reviewed cached inventory and the
# three short direct bodies in ppc_recomp.0.cpp. Only these six files/locations
# are read; no recursive PPC discovery or content hash is involved.
SOURCES = {
    "82296830": ("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp", 15523),
    "8229C8B0": ("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp", 30144),
    "8229F5E0": ("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp", 37348),
    "824070C8": ("LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp", 46535),
    "82722D18": ("LostOdysseyRecompLib/ppc/ppc_recomp.67.cpp", 25343),
    "82723DB8": ("LostOdysseyRecompLib/ppc/ppc_recomp.67.cpp", 27798),
}
CALLEES = {
    "82296830": [],
    "8229C8B0": ["82296830", "8229F678", "82B7A0B0"],
    "8229F5E0": ["82296830", "8229F678", "82B7A0B0"],
    "824070C8": ["822C42D8", "8229C8B0", "8240B1B8"],
    "82722D18": ["8229F5E0", "825F41E8"],
    "82723DB8": ["8229F5E0"],
}
PARAMETERS = {
    "82296830": {"kind": "utf16_length", "step": 2},
    "8229C8B0": {"kind": "initialize", "element_size": 2,
                 "resize_argument": 8, "frame_size": 112,
                 "field_write_order": [4, 8, 0]},
    "8229F5E0": {"kind": "assign", "element_size": 2,
                 "resize_argument": 8, "frame_size": 112,
                 "field_write_order": [8, 4], "self_pointer_guard": True},
    "824070C8": {"kind": "registration", "element_size": 12,
                 "resize_argument": 8, "frame_size": 112,
                 "source_pointer": "0x821909C8", "singleton": "8240B1B8",
                 "clear_flag_mask": "0x80000000", "set_flag_mask": "0x20000000"},
    "82722D18": {"kind": "metadata", "frame_size": 112,
                 "source_pointer": "0x821A83D0", "metadata_word": "0x0001003C",
                 "outgoing_offset": 80},
    "82723DB8": {"kind": "tail_assign", "source_pointer": "0x82201354",
                 "destination_offset": 72},
}


def extract(address, path, line, source_root):
    lines = (source_root / path).read_text(encoding="utf-8").splitlines(keepends=True)
    start = line - 1
    marker = f"PPC_FUNC_IMPL(__imp__sub_{address}) {{"
    if lines[start].rstrip() != marker:
        raise ValueError(f"{address}: fixed source line changed")
    end = next((index for index in range(start + 1, min(start + 300, len(lines)))
                if lines[index].rstrip() == "}"), None)
    if end is None:
        raise ValueError(f"{address}: body end was not found in bounded source")
    body = "".join(lines[start:end + 1])
    instructions = [instruction.strip() for instruction in re.findall(
        r"^\s*// (.+)$", body, re.MULTILINE)]
    labels = re.findall(r"^(loc_[0-9A-Fa-f]+):", body, re.MULTILINE)
    branch_edges = [instruction for instruction in instructions
                    if re.match(r"(?:b|beq|bne|blt|bgt|ble|bge)\s", instruction)]
    callees = [target.upper() for target in re.findall(
        r"\bsub_([0-9A-Fa-f]{8})\(ctx, base\);", body)]
    if sorted(set(callees)) != sorted(CALLEES[address]):
        raise ValueError(f"{address}: dependency set changed: {callees}")
    if len(instructions) < 4 or (instructions[-1] != "blr" and
                                 not (address == "82723DB8" and
                                      instructions[-1] == "b 0x8229f5e0")):
        raise ValueError(f"{address}: incomplete instruction list")
    return {"address": address, "source": path, "source_line": line,
            "parameters": PARAMETERS[address], "callees": callees,
            "cfg": {"labels": labels, "branch_instructions": branch_edges},
            "instructions": instructions, "translated_body": body}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--write-reviewed-baseline", action="store_true")
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="checkout containing generated PPC source")
    args = parser.parse_args()
    entries = [extract(address, *source, args.source_root) for address, source in SOURCES.items()]
    payload = {"schema_version": 1, "family": "registered_metadata_string",
               "entry_count": 6, "entries": entries}
    rendered = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"
    if args.write_reviewed_baseline:
        OUTPUT.write_text(rendered, encoding="utf-8", newline="\n")
    elif not OUTPUT.exists() or OUTPUT.read_text(encoding="utf-8") != rendered:
        raise ValueError("Reviewed six-body manifest differs from exact PPC source")
    print("Verified 6 complete translated PPC bodies, CFG edges, callees and parameters")


if __name__ == "__main__":
    main()
