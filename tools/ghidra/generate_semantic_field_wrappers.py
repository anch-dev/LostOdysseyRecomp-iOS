"""Generate opt-in wrappers for recovered field bit and arithmetic operations."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
FAMILIES = {"field_bits": "Bits", "field_arithmetic": "Arithmetic"}


def generate(manifest: dict, family: str) -> str:
    if family not in FAMILIES or manifest.get("schema_version") != 1 or \
            manifest.get("kind") != f"strict_{family}_semantics":
        raise ValueError("unsupported field semantic manifest")
    entries = manifest.get("entries")
    if not isinstance(entries, list) or not entries:
        raise ValueError("empty field manifest")
    seen = set()
    lines = [f"// Generated from {family}_families.json; do not edit.",
             '#include "cpu/semantic_fields.h"', ""]
    for entry in entries:
        address = entry.get("address")
        if not isinstance(address, str) or not re.fullmatch(r"[0-9A-F]{8}", address) or address in seen:
            raise ValueError(f"invalid or duplicate field address: {address}")
        seen.add(address)
        symbol = f"sub_{address}"
        lines.extend([f'extern "C" PPC_FUNC(__imp__{symbol});',
                      f"PPC_FUNC({symbol})", "{",
                      f"    lo::runtime::semantic_fields::Dispatch{FAMILIES[family]}(",
                      f"        0x{address}u, ctx, base, &__imp__{symbol});", "}", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--family", choices=FAMILIES, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = ROOT / f"LostOdysseyRecompSemantics/{args.family}_families.json"
    source = generate(json.loads(manifest.read_text(encoding="utf-8")), args.family)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.is_file() or args.output.read_text(encoding="utf-8") != source:
        args.output.write_text(source, encoding="utf-8")
    print(f"Generated {source.count('PPC_FUNC(sub_')} {args.family} wrappers")


if __name__ == "__main__":
    main()
