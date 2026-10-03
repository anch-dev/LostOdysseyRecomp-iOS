"""Accept the complete reviewed 823FD400 member-chain dispatch body."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from semantic_batch import ROOT, extract_originals

MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_member_dispatch_families.json"
SPEC = {"address": "823FD400",
        "generated_ppc_path": "LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp",
        "line": 23236}


def generate(ppc_root: Path) -> dict:
    body = extract_originals([SPEC], ppc_root).decode("utf-8")
    instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
    if len(instructions) != 31 or body.count("PPC_CALL_INDIRECT_FUNC(") != 1 or \
            re.search(r"\bsub_[0-9A-F]{8}\(ctx, base\);", body):
        raise ValueError("member dispatch body/calls changed")
    entry = {**SPEC, "instruction_count": len(instructions),
             "instruction_sequence": instructions, "translated_body": body,
             "direct_calls": [], "indirect_calls": ["live member vtable+340"],
             "cfg": {"labels": re.findall(r"^(loc_[0-9A-F]+):", body, re.M),
                     "branches": [i for i in instructions if re.match(r"^b[a-z]*(?:\s|$)", i)]},
             "parameters": {"r3": "full destination receiver",
                            "r4": "full type pointer with member list at +120",
                            "caller_sp": "full entry SP",
                            "result": "full live r3, last method result or entry receiver"}}
    return {"schema_version": 1, "family": "metadata_member_dispatch",
            "entry_count": 1, "entries": [entry], "limitations": [
                "The actual dynamic vtable+340 target and full r3/r4/SP/LR are exposed as an explicit service.",
                "Selected live r30/r31/CTR/LR and own saved frame are modeled; other volatile ABI effects and target internals are external.",
                "Ordinary mapped RAM only; faults/MMIO and concurrent mutation excluded."]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=Path.home() /
                        "ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
    args = parser.parse_args()
    if json.loads(MANIFEST.read_text(encoding="utf-8")) != generate(args.ppc_root):
        raise ValueError("member dispatch reviewed full-body manifest changed")
    print("metadata-member-dispatch: 1 exact PPC body accepted")


if __name__ == "__main__":
    main()
