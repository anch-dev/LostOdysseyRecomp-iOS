"""Create draft pins from explicitly selected complete PPC bodies.

Usage: semantic_pin.py --entry 82B82728=ppc_recomp.176.cpp --manifest draft.json
This records evidence only; it neither generates semantics nor awards mapping credit.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
from semantic_recovery import ADDRESS, SOURCE, DEFAULT_PPC, _metadata


def pin_entries(selections: list[str], ppc_root: Path) -> list[dict]:
    cache: dict[str, str] = {}
    entries = []
    seen = set()
    for selection in selections:
        address, separator, name = selection.partition("=")
        if not separator or not ADDRESS.fullmatch(address) or not SOURCE.fullmatch(name):
            raise ValueError(f"expected ADDRESS=ppc_recomp.N.cpp: {selection}")
        address = address.upper()
        if address in seen:
            raise ValueError(f"duplicate selected entry: {address}")
        seen.add(address)
        if name not in cache:
            cache[name] = (ppc_root / name).read_text(encoding="utf-8")
        text = cache[name]
        start = text.index(f"PPC_FUNC_IMPL(__imp__sub_{address})")
        end = text.index("\n}", start) + 2
        body = text[start:end]
        instructions = re.findall(r"^\s*// (.*)$", body, re.M)
        entry = {"address": address, "source": f"LostOdysseyRecompLib/ppc/{name}",
                 "source_line": text[:start].count("\n") + 1,
                 "instruction_count": len(instructions), "instructions": instructions,
                 "translated_body": body, "status": "validation_pending"}
        _metadata(entry, body)
        entries.append(entry)
    return entries


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--entry", nargs="+", required=True)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--schema", default="semantic-recovery-draft-v1")
    args = parser.parse_args()
    if args.manifest.name.endswith("families.json"):
        parser.error("pin output must be a draft, not a canonical family manifest")
    entries = pin_entries(args.entry, args.ppc_root)
    document = {"schema": args.schema, "entries": entries,
                "validation": {"status": "draft", "complete": False,
                               "runtime_validated": False}}
    with args.manifest.open("x", encoding="utf-8") as output:
        json.dump(document, output, indent=2)
        output.write("\n")
    print(json.dumps({"manifest": str(args.manifest),
                      "entries": [{"address": item["address"],
                                   "instructions": item["instruction_count"]}
                                  for item in entries]}, indent=2))


if __name__ == "__main__":
    main()
