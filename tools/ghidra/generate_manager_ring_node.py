"""Admit the exact ring/heap node composition and save28 helpers."""

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "LostOdysseyRecompSemantics/manager_ring_node_families.json"
SOURCES = {"82326580": ("ppc_recomp.5.cpp", 48819, 67)}
CALLEES = {"82326580": ["82486C88", "82290AB8", "82700FD8", "82486C88", "82700FD8"]}
PARAMETERS = {"82326580": {"frame_bytes": 128,
    "mode_global": "83318040", "ring": "8336A7A4",
    "ring_record_bytes": 20, "node_bytes": 144,
    "ring_record_vtable": "820010C4", "ring_node_vtable": "820011D4",
    "heap_node_vtable": "822096D0", "method_slot": 4,
    "final_callback_lr": "82326678", "zero_address_reads": "unconditional"}}



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
    return {"schema_version": 1, "family": "manager_ring_node",
            "entry_count": 1, "entries": entries, "abi_helpers": helpers}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    parser.add_argument("--write-reviewed-baseline", action="store_true")
    args = parser.parse_args()
    rendered = json.dumps(reviewed(args.source_root), indent=2) + "\n"
    if args.write_reviewed_baseline:
        OUTPUT.write_text(rendered, encoding="utf-8", newline="\n")
    elif OUTPUT.read_text(encoding="utf-8") != rendered:
        raise ValueError("ring node baseline differs from exact PPC")
    print("Verified 1 complete ring node body and 2 actual ABI helpers")


if __name__ == "__main__":
    main()
