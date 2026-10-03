"""Generate opt-in runtime wrappers from the reviewed registered getter table."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
HEADER = SEMANTICS / "include/lo_semantics/registered_getter_family.h"
ADDRESS = re.compile(r"[0-9A-F]{8}")


def checked_entries(manifest: dict) -> list[dict]:
    entries = manifest.get("entries")
    if manifest.get("schema_version") != 1 or \
            manifest.get("family") != "registered_getter" or \
            manifest.get("entry_count") != 58 or \
            not isinstance(entries, list) or len(entries) != 58:
        raise ValueError("reviewed 58-entry getter manifest changed")
    addresses = [entry.get("address") for entry in entries]
    if any(not isinstance(address, str) or not ADDRESS.fullmatch(address)
           for address in addresses) or addresses != sorted(set(addresses)):
        raise ValueError("invalid or duplicate getter address")
    rows = []
    for entry in entries:
        if entry.get("frame_size") != 96 or any(
                not isinstance(entry.get(field), str) or
                not ADDRESS.fullmatch(entry[field])
                for field in ("constructor", "registration")) or \
                not re.fullmatch(r"0x[0-9A-F]{8}", entry.get("singleton_address", "")) or \
                not re.fullmatch(r"0xFFFFFFFF[0-9A-F]{8}", entry.get("owner", "")):
            raise ValueError(f"unreviewed getter parameters: {entry['address']}")
        rows.append(f"{{0x{entry['address']}u, {entry['singleton_address']}u, "
                    f"0x{entry['constructor']}u, 0x{entry['registration']}u, "
                    f"{entry['owner']}ull}},")
    source = HEADER.read_text(encoding="utf-8")
    begin = "// BEGIN GENERATED REGISTERED GETTER PARAMETERS"
    end = "// END GENERATED REGISTERED GETTER PARAMETERS"
    if source.count(begin) != 1 or source.count(end) != 1:
        raise ValueError("shared getter table markers changed")
    table = source.split(begin, 1)[1].split(end, 1)[0]
    if re.sub(r"\s+", "", table) != re.sub(r"\s+", "", "".join(rows)):
        raise ValueError("shared getter table differs from the reviewed manifest")
    return entries


def preflight_symbols(entries: list[dict]) -> None:
    addresses = {entry["address"] for entry in entries}
    for path in SEMANTICS.glob("*_families.json"):
        if path.name == "registered_getter_families.json":
            continue
        mapped = {entry["address"] for entry in
                  json.loads(path.read_text(encoding="utf-8"))["entries"]}
        if overlap := addresses & mapped:
            raise ValueError(f"getter overlap with {path.name}: {sorted(overlap)}")
    # Only hand-written runtime sources, never the private generated PPC tree.
    pattern = re.compile(r"PPC_FUNC(?:_IMPL)?\(sub_([0-9A-F]{8})\)")
    for path in (ROOT / "LostOdysseyRecomp").rglob("*.cpp"):
        if overlap := addresses & set(pattern.findall(path.read_text(encoding="utf-8"))):
            raise ValueError(f"getter hook collision in {path}: {sorted(overlap)}")


def generate(manifest: dict) -> str:
    entries = checked_entries(manifest)
    preflight_symbols(entries)
    lines = ['// Generated from registered_getter_families.json; do not edit.',
             '#include "cpu/semantic_registered.h"', ""]
    for entry in entries:
        symbol = f"sub_{entry['address']}"
        lines.extend([
            f'extern "C" PPC_FUNC(__imp__{symbol});',
            f"PPC_FUNC({symbol})", "{",
            "    if (!lo::runtime::semantic_registered::Enabled() ||",
            "        !lo::runtime::semantic_registered::Apply(ctx, base, "
            f"0x{entry['address']}u))",
            f"        __imp__{symbol}(ctx, base);",
            "}", "",
        ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=SEMANTICS /
                        "registered_getter_families.json")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = generate(json.loads(args.manifest.read_text(encoding="utf-8")))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != source:
        args.output.write_text(source, encoding="utf-8", newline="\n")
    print("Generated 58 semantic registered getter wrappers")


if __name__ == "__main__":
    main()
