"""Pin complete descriptor slot removal/change PPC bodies at recorded lines."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_descriptor_membership_families.json"
SPECS = (
    ("824080A8", "ppc_recomp.15.cpp", 48946, 65, ["82298AF8"], 2),
    ("823AAE00", "ppc_recomp.11.cpp", 22661, 59, ["824080A8", "825F41E8"], 1),
)


def generate(ppc_root: Path) -> dict:
    entries = []
    for address, name, line, count, calls, indirect in SPECS:
        spec = {"address": address, "generated_ppc_path":
                "LostOdysseyRecompLib/ppc/" + name, "line": line}
        body = extract_originals([spec], ppc_root).decode("utf-8")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        if len(instructions) != count or re.findall(
                r"\bsub_([0-9A-F]{8})\(ctx, base\);", body) != calls or \
                body.count("PPC_CALL_INDIRECT_FUNC(") != indirect:
            raise ValueError(f"membership complete body/calls changed: {address}")
        entries.append({**spec, "instruction_count": count,
            "instruction_sequence": instructions, "translated_body": body,
            "direct_calls": calls, "indirect_call_count": indirect,
            "cfg": {"labels": re.findall(r"^(loc_[0-9A-F]+):", body, re.M),
                    "branches": [i for i in instructions if re.match(r"^b[a-z]*(?:\s|$)", i)]},
            "parameters": {"r3": "full owner/member pointer", "r4": "full member pointer/slot index",
                           "caller_sp": "full entry SP", "result": "full live r3"}})
    return {"schema_version": 1, "family": "metadata_descriptor_membership",
            "entry_count": len(entries), "entries": entries, "limitations": [
                "Actual parent calls the recovered child; array removal/append algorithms are reused, not opaque stand-ins.",
                "Observer vtable+4/+8/+12 dispatch exposes full receiver/argument/SP, selected live LR/GPR27-31/CTR and full r3 result.",
                "Dynamic target internals and unexposed generic lower ABI/frame effects remain external to existing APIs.",
                "Ordinary RAM only; faults/MMIO, access widths and concurrent mutation excluded."]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
                        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    expected = generate(args.ppc_root)
    if args.write:
        MANIFEST.write_text(json.dumps(expected, indent=2) + "\n", encoding="utf-8")
    elif json.loads(MANIFEST.read_text(encoding="utf-8")) != expected:
        raise ValueError("membership reviewed full-body manifest changed")
    print("metadata-descriptor-membership: 2 exact PPC bodies accepted")


if __name__ == "__main__":
    main()
