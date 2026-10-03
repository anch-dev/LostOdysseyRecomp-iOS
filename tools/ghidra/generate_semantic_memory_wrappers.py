"""Generate opt-in PPC wrappers for recovered memory operation families."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


FAMILY_KIND = {
    "read_only_fields": "strict_read_only_field_semantics",
    "memory_writes": "memory_write_semantics",
    "single_write_fields": "single_write_field_semantics",
}
# Keep the order of aggregate initialization aligned with each Registers struct.
REGISTERS = {
    "read_only_fields": (3, 4, 5, 8, 9, 10, 11, 13, 18),
    "memory_writes": (3, 4, 5, 6, 7, 8, 9, 10, 11, 1),
    "single_write_fields": (3, 4, 5, 6, 9, 10, 11),
}
ADDRESS = re.compile(r"[0-9A-F]{8}")
REFERENCED_REGISTER = re.compile(r"ctx\.r([0-9]+)")


def checked_entries(manifest: dict, family: str) -> list[dict]:
    if family not in FAMILY_KIND or manifest.get("schema_version") != 1 or \
            manifest.get("kind") != FAMILY_KIND[family]:
        raise ValueError(f"unsupported {family} manifest")
    entries = manifest.get("entries")
    counts = manifest.get("family_counts")
    if not isinstance(entries, list) or not entries or not isinstance(counts, dict):
        raise ValueError(f"empty or incomplete {family} manifest")
    addresses = [entry.get("address") for entry in entries]
    if any(not isinstance(address, str) or not ADDRESS.fullmatch(address)
           for address in addresses) or addresses != sorted(set(addresses)):
        raise ValueError(f"invalid or duplicate {family} address")
    if sum(counts.values()) != len(entries) or any(
            sum(entry.get("family") == name for entry in entries) != count
            for name, count in counts.items()):
        raise ValueError(f"{family} family counts changed")
    allowed = set(REGISTERS[family])
    for entry in entries:
        effects = entry.get("instruction_effects")
        statements = entry.get("generated_statements")
        if not isinstance(effects, list) or not effects or \
                [effect.get("instruction") for effect in effects] != entry.get("instruction_sequence") or \
                any(len(effect.get("statements", [])) != 1 for effect in effects) or \
                effects[-1]["statements"] != ["return;"] or \
                statements != ["PPC_FUNC_PROLOGUE();", *(effect["statements"][0] for effect in effects)]:
            raise ValueError(f"unreviewed instruction effects at {entry['address']}")
        referenced = {int(number) for statement in statements
                      for number in REFERENCED_REGISTER.findall(statement)}
        if not referenced <= allowed:
            raise ValueError(f"unreviewed register at {entry['address']}: {referenced - allowed}")
    return entries


def generate(manifest: dict, family: str) -> str:
    """Return a translation unit with strong wrappers for a reviewed family."""
    entries = checked_entries(manifest, family)
    lines = [
        f"// Generated from {FAMILY_KIND[family]}; do not edit.",
        '#include "cpu/semantic_memory.h"',
        "",
    ]
    for entry in entries:
        address = entry["address"]
        symbol = f"sub_{address}"
        lines.extend([
            f'extern "C" PPC_FUNC(__imp__{symbol});',
            f"PPC_FUNC({symbol})",
            "{",
            "    if (!lo::runtime::semantic_memory::Enabled())",
            "    {",
            f"        __imp__{symbol}(ctx, base);",
            "        return;",
            "    }",
            "    lo::runtime::semantic_memory::NativeMemory memory(base);",
            f"    lo::semantic::{family}::Registers registers{{",
            *[f"        .r{number} = ctx.r{number}.u64," for number in REGISTERS[family]],
            "    };",
            f"    if (!lo::semantic::{family}::ApplyWith(0x{address}u, registers, memory))",
            "        std::abort();",
            *[f"    ctx.r{number}.u64 = registers.r{number};" for number in REGISTERS[family]],
            "}",
            "",
        ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--read-manifest", type=Path, required=True)
    parser.add_argument("--write-manifest", type=Path, required=True)
    parser.add_argument("--single-manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = "\n".join(
        generate(json.loads(path.read_text(encoding="utf-8")), family)
        for family, path in (
            ("read_only_fields", args.read_manifest),
            ("memory_writes", args.write_manifest),
            ("single_write_fields", args.single_manifest),
        )
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.is_file() or args.output.read_text(encoding="utf-8") != source:
        args.output.write_text(source, encoding="utf-8")
    print(f"Generated {source.count('PPC_FUNC(sub_')} semantic memory wrappers")


if __name__ == "__main__":
    main()
