"""Generate the reviewed 655 opt-in integer instance initializer wrappers."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import (
    checked_ordered_spec, checked_single_spec, raw_bodies, write_if_changed,
)
from generate_instance_field_initializer_family import checked_spec as field_spec
from generate_instance_property_initializer_family import checked_spec as property_spec
from generate_instance_marker_initializer_family import checked_spec as marker_spec
from generate_instance_scalar_initializer_family import recover as scalar_spec
from generate_instance_ui_initializer_family import checked_spec as ui_spec


ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
ABI_PATH = SEMANTICS / "instance_runtime_abi.json"
FAMILIES = (
    ("instance_vtable_families.json", 583),
    ("instance_field_initializer_families.json", 19),
    ("instance_property_initializer_families.json", 15),
    ("instance_marker_initializer_families.json", 13),
    ("instance_scalar_initializer_families.json", 19),
    ("instance_ui_initializer_families.json", 6),
)
ADDRESS = re.compile(r"[0-9A-F]{8}")


def family_entries() -> dict[str, tuple[str, dict]]:
    selected = {}
    for filename, count in FAMILIES:
        manifest = json.loads((SEMANTICS / filename).read_text(encoding="utf-8"))
        entries = manifest.get("entries")
        if manifest.get("schema_version") != 1 or \
                manifest.get("entry_count") != count or \
                not isinstance(entries, list) or len(entries) != count:
            raise ValueError(f"reviewed family count changed: {filename}")
        addresses = [item.get("address") for item in entries]
        if any(not isinstance(value, str) or not ADDRESS.fullmatch(value)
               for value in addresses) or addresses != sorted(set(addresses)):
            raise ValueError(f"invalid family addresses: {filename}")
        for item in entries:
            address = item["address"]
            if address in selected:
                raise ValueError(f"duplicate instance address: {address}")
            selected[address] = (filename, item)
    if len(selected) != 655:
        raise ValueError("655-entry instance selection changed")
    return dict(sorted(selected.items()))


def recovered(filename: str, candidate: dict, body: list[str]) -> dict:
    if filename == "instance_vtable_families.json":
        return (checked_single_spec(candidate, body) or
                checked_ordered_spec(candidate, body))
    if filename == "instance_field_initializer_families.json":
        return field_spec(candidate, body)
    if filename == "instance_property_initializer_families.json":
        return property_spec(candidate, body)
    if filename == "instance_marker_initializer_families.json":
        return marker_spec(candidate, body)
    if filename == "instance_scalar_initializer_families.json":
        return scalar_spec(candidate, body, next(
            name for name, targets in __import__(
                "generate_instance_scalar_initializer_family").GROUPS.items()
            if candidate["address"] in targets))
    if filename == "instance_ui_initializer_families.json":
        return ui_spec(candidate, body)
    raise ValueError(filename)


def abi_prefix(instructions: list[str], address: str) -> dict[str, int]:
    if instructions[:2] != ["cmplwi cr6,r3,0", "beqlr cr6"] or \
            instructions[-1] != "blr":
        raise ValueError(f"instance guard/return changed: {address}")
    registers = {}
    first_store = False
    for instruction in instructions[2:-1]:
        if instruction.startswith(("stw ", "stb ")):
            first_store = True
            continue
        if first_store:
            raise ValueError(f"interleaved register assignment: {address}")
        if match := re.fullmatch(r"lis (r(?:[7-9]|10|11)),(-?\d+)", instruction):
            target, immediate = match.groups()
            value = int(immediate) << 16
        elif match := re.fullmatch(r"li (r(?:[7-9]|10|11)),(-?\d+)", instruction):
            target, immediate = match.groups()
            value = int(immediate)
        elif match := re.fullmatch(r"addi (r(?:[7-9]|10|11)),"
                                   r"(r(?:[7-9]|10|11)),(-?\d+)", instruction):
            target, source, immediate = match.groups()
            if source not in registers:
                raise ValueError(f"uninitialized register in {address}")
            value = registers[source] + int(immediate)
        else:
            raise ValueError(f"unreviewed prefix instruction in {address}: {instruction}")
        if not -(1 << 63) <= value < (1 << 63):
            raise ValueError(f"prefix overflow: {address}")
        registers[target] = value
    if not first_store or not registers:
        raise ValueError(f"missing instance stores/registers: {address}")
    return dict(sorted(registers.items(), key=lambda pair: int(pair[0][1:])))


def refreshed_abi(entries: dict[str, tuple[str, dict]]) -> dict:
    cache = ROOT / "out/function-inventory"
    candidates = json.loads((cache / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    bodies = raw_bodies(cache / "registered-instance-originals.cpp.gz")
    indexed = {item["address"]: item for item in candidates["entries"]}
    if len(indexed) != len(bodies) or len(bodies) != 786:
        raise ValueError("cached original instance inventory changed")
    rows = []
    for address, (filename, manifest_entry) in entries.items():
        candidate = indexed[address]
        if recovered(filename, candidate, bodies[address]) != manifest_entry:
            raise ValueError(f"checked family parameters differ: {address}")
        rows.append({"address": address, "registers":
                     abi_prefix(candidate["instructions"], address)})
    return {"schema_version": 1, "family": "instance_runtime_abi",
            "entry_count": len(rows), "entries": rows}


def checked_abi(entries: dict[str, tuple[str, dict]], path: Path) -> list[dict]:
    abi = json.loads(path.read_text(encoding="utf-8"))
    rows = abi.get("entries")
    if abi.get("schema_version") != 1 or \
            abi.get("family") != "instance_runtime_abi" or \
            abi.get("entry_count") != 655 or \
            not isinstance(rows, list) or len(rows) != 655 or \
            [row.get("address") for row in rows] != list(entries):
        raise ValueError("reviewed instance ABI table changed")
    for row in rows:
        regs = row.get("registers")
        if not isinstance(regs, dict) or not regs or any(
                register not in {f"r{i}" for i in range(7, 12)} or
                not isinstance(value, int) or isinstance(value, bool) or
                not -(1 << 63) <= value < (1 << 63)
                for register, value in regs.items()):
            raise ValueError(f"invalid instance ABI register: {row['address']}")
    return rows


def preflight(addresses: set[str]) -> None:
    own = {filename for filename, _ in FAMILIES}
    for path in SEMANTICS.glob("*_families.json"):
        if path.name in own:
            continue
        manifest = json.loads(path.read_text(encoding="utf-8"))
        other = {item["address"] for item in manifest.get("entries", [])}
        if overlap := addresses & other:
            raise ValueError(f"instance overlap with {path.name}: {sorted(overlap)}")
    pattern = re.compile(r"PPC_FUNC(?:_IMPL)?\(sub_([0-9A-F]{8})\)")
    for path in (ROOT / "LostOdysseyRecomp").rglob("*.cpp"):
        if overlap := addresses & set(pattern.findall(path.read_text(encoding="utf-8"))):
            raise ValueError(f"instance hook collision in {path}: {sorted(overlap)}")


def generate(rows: list[dict]) -> str:
    lines = ["// Generated from reviewed instance_runtime_abi.json; do not edit.",
             '#include "cpu/semantic_instance.h"',
             "#include <algorithm>", "#include <iterator>", "",
             "namespace lo::runtime::semantic_instance {", "namespace {",
             "constexpr AbiSpec kSpecs[] = {"]
    for row in rows:
        regs = row["registers"]
        mask = sum(1 << (int(register[1:]) - 7) for register in regs)
        values = ", ".join(f"{regs.get(f'r{number}', 0)}ll" for number in range(7, 12))
        lines.append(f"    {{0x{row['address']}u, 0x{mask:02x}u, {{{values}}}}},")
    lines.extend(["};", "} // namespace", "",
                  "const AbiSpec* FindAbi(std::uint32_t address) noexcept",
                  "{",
                  "    const auto* entry = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),",
                  "        address, [](const AbiSpec& row, std::uint32_t key)",
                  "        { return row.address < key; });",
                  "    return entry != std::end(kSpecs) && entry->address == address ? entry : nullptr;",
                  "}",
                  "} // namespace lo::runtime::semantic_instance", ""])
    for row in rows:
        address = row["address"]
        symbol = f"sub_{address}"
        lines.extend([f'extern "C" PPC_FUNC(__imp__{symbol});',
                      f"PPC_FUNC({symbol})", "{",
                      "    if (!lo::runtime::semantic_instance::Enabled() ||",
                      f"        !lo::runtime::semantic_instance::Apply(ctx, base, 0x{address}u))",
                      f"        __imp__{symbol}(ctx, base);",
                      "}", ""])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--refresh-abi", action="store_true")
    parser.add_argument("--check-abi", action="store_true")
    args = parser.parse_args()
    entries = family_entries()
    if args.refresh_abi or args.check_abi:
        expected = json.dumps(refreshed_abi(entries), indent=2) + "\n"
        if args.check_abi:
            if ABI_PATH.read_text(encoding="utf-8") != expected:
                raise ValueError("checked instance runtime ABI differs from cached PPC")
        else:
            write_if_changed(ABI_PATH, expected)
    rows = checked_abi(entries, ABI_PATH)
    preflight(set(entries))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write_if_changed(args.output, generate(rows))
    elif not (args.refresh_abi or args.check_abi):
        parser.error("--output is required unless checking or refreshing ABI")
    print("validated 655 semantic instance wrappers")


if __name__ == "__main__":
    main()
