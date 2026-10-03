"""Admit three exact bit-array/World bodies from fixed PPC source positions."""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "LostOdysseyRecompSemantics/instance_allocation_composed_families.json"
SOURCES = {
    "823058F0": ("ppc_recomp.4.cpp", 30967, 30),
    "825BA620": ("ppc_recomp.43.cpp", 24269, 36),
    "825BA858": ("ppc_recomp.43.cpp", 24630, 40),
}
CALLEES = {"823058F0": ["827C5F38"],
           "825BA620": ["823058F0", "82B7BC40"],
           "825BA858": ["82496948", "825BA620", "825BA620"]}
PARAMETERS = {
    "823058F0": {"kind": "bit_word_resize", "frame_bytes": 128,
        "manager_global": "8330B608", "vtable_slot": 8,
        "alignment": 8, "byte_count": "low32(trunc(signed32(bits+31)/32)*4)",
        "full_callback_return": True},
    "825BA620": {"kind": "bit_array_holder", "frame_bytes": 112,
        "array_offset": 24, "clear_offsets": list(range(0, 36, 4))},
    "825BA858": {"kind": "world_composition", "frame_bytes": 112,
        "initial_vtable60": "82202A80", "vtable0": "821DAF10",
        "vtable60": "821DB01C", "property_offset": 212,
        "holder_offsets": [388, 424], "return": "second_holder_full_r3"},
}


def extract(source_root, filename, line, symbol):
    lines = (source_root / "LostOdysseyRecompLib/ppc" / filename).read_text(
        encoding="utf-8").splitlines(keepends=True)
    start = line - 1
    if lines[start].rstrip() != f"PPC_FUNC_IMPL(__imp__{symbol}) {{":
        raise ValueError(f"fixed PPC position changed: {symbol}")
    end = next((i for i in range(start + 1, min(start + 450, len(lines)))
                if lines[i].rstrip() == "}"), None)
    if end is None:
        raise ValueError(f"incomplete body: {symbol}")
    body = "".join(lines[start:end + 1])
    instructions = [x.strip() for x in re.findall(r"^\s*// (.+)$", body, re.M)]
    return {"source": f"LostOdysseyRecompLib/ppc/{filename}",
            "source_line": line, "translated_body": body,
            "instructions": instructions,
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-Fa-f]+):", body, re.M),
                    "branches": [x for x in instructions if x.startswith("b")]},
            "callees": [x.upper() for x in re.findall(
                r"\bsub_([0-9A-Fa-f]{8})\(ctx, base\);", body)]}


def reviewed(source_root):
    entries = []
    for address, (filename, line, count) in SOURCES.items():
        body = extract(source_root, filename, line, f"sub_{address}")
        if len(body["instructions"]) != count or body["callees"] != CALLEES[address]:
            raise ValueError(f"PPC instruction or callee set changed: {address}")
        entries.append({"address": address, "parameters": PARAMETERS[address], **body})
    helpers = [{"name": name,
                **extract(source_root, "ppc_recomp.175.cpp", line, name)}
               for name, line in (("__savegprlr_28", 5671),
                                  ("__restgprlr_28", 6257))]
    return {"schema_version": 1, "family": "instance_allocation_composed",
            "entry_count": 3, "entries": entries, "abi_helpers": helpers}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--write-reviewed-baseline", action="store_true")
    args = parser.parse_args()
    rendered = json.dumps(reviewed(args.source_root), indent=2) + "\n"
    if args.write_reviewed_baseline:
        OUTPUT.write_text(rendered, encoding="utf-8", newline="\n")
    elif OUTPUT.read_text(encoding="utf-8") != rendered:
        raise ValueError("bit-array/World baseline differs from exact PPC")
    print("Verified 3 complete bit-array/World bodies and 2 actual ABI helpers")


if __name__ == "__main__":
    main()
