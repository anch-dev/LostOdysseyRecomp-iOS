"""Validate and emit the six exact UI instance initialization bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED INSTANCE UI PARAMETERS\n"
END = "    // END GENERATED INSTANCE UI PARAMETERS\n"
TARGETS = (
    "82631AD0", "82631BE8", "826320A0", "82632250", "82632480", "82632648",
)
PREFIX = [
    "cmplwi cr6,r3,0", "beqlr cr6",
    "lis r11,-32228", "lis r10,-32225", "lis r9,-32225",
    "addi r11,r11,-21160",
]


def required(pattern: str, instruction: str, address: str) -> int:
    match = re.fullmatch(pattern, instruction)
    if match is None:
        raise ValueError(f"UI instruction changed at {address}: {instruction}")
    return int(match.group(1))


def checked_spec(entry: dict, body: list[str]) -> dict:
    address = entry["address"]
    ins = entry["instructions"]
    if len(ins) != 15 or ins[:6] != PREFIX or ins[8] != "li r8,0" or \
            ins[10] != "stw r10,0(r3)" or ins[14] != "blr":
        raise ValueError(f"UI body skeleton changed: {address}")
    r10_lo = required(r"addi r10,r10,(-?\d+)", ins[6], address)
    r9_lo = required(r"addi r9,r9,(-?\d+)", ins[7], address)
    first = required(r"stw r11,(\d+)\(r3\)", ins[9], address)
    second = required(r"stw r9,(\d+)\(r3\)", ins[11], address)
    zero = required(r"stw r8,(\d+)\(r3\)", ins[12], address)
    final_zero = required(r"stw r8,(\d+)\(r3\)", ins[13], address)
    if first != second or final_zero != zero + 4 or \
            (first, zero) not in {(656, 660), (608, 612),
                                  (608, 752), (608, 748)}:
        raise ValueError(f"UI field layout or write order changed: {address}")
    if entry["cfg_branches"] or entry["direct_calls"]:
        raise ValueError(f"UI call/control flow changed: {address}")

    expected = [
        f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
        "PPC_FUNC_PROLOGUE();",
        f"// {ins[0]}",
        "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
        f"// {ins[1]}",
        "if (ctx.cr6.eq) return;",
    ]
    for index, register, value in ((2, 11, -32228 << 16),
                                   (3, 10, -32225 << 16),
                                   (4, 9, -32225 << 16)):
        expected += [f"// {ins[index]}", f"ctx.r{register}.s64 = {value};"]
    for index, register, value in ((5, 11, -21160),
                                   (6, 10, r10_lo), (7, 9, r9_lo)):
        expected += [f"// {ins[index]}",
                     f"ctx.r{register}.s64 = ctx.r{register}.s64 + {value};"]
    expected += [f"// {ins[8]}", "ctx.r8.s64 = 0;"]
    for index, register, offset in ((9, 11, first), (10, 10, 0),
                                    (11, 9, first), (12, 8, zero),
                                    (13, 8, zero + 4)):
        expected += [f"// {ins[index]}",
                     f"PPC_STORE_U32(ctx.r3.u32 + {offset}, ctx.r{register}.u32);"]
    expected += [f"// {ins[14]}", "return;", "}"]
    if body != expected:
        raise ValueError(f"full generated UI body changed: {address}")
    return {
        "address": address,
        "first_field_offset": first,
        "zero_field_offset": zero,
        "initial_field_word": signed_word(-32228, -21160, address),
        "vtable": signed_word(-32225, r10_lo, address),
        "final_field_word": signed_word(-32225, r9_lo, address),
        "source": entry["generated_ppc_path"],
        "source_line": entry["line"],
        "registered_types": entry["types"],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidates", type=Path, default=ROOT /
                        "out/function-inventory/registered-instance-candidates.json")
    parser.add_argument("--originals", type=Path, default=ROOT /
                        "out/function-inventory/registered-instance-originals.cpp.gz")
    parser.add_argument("--manifest", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/instance_ui_initializer_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/instance_ui_initializer_family.cpp")
    args = parser.parse_args()

    candidates = json.loads(args.candidates.read_text(encoding="utf-8"))
    bodies = raw_bodies(args.originals)
    if len(bodies) != 786 or len(candidates["entries"]) != 786:
        raise ValueError("outgoing92 original inventory changed")
    by_address = {entry["address"]: entry for entry in candidates["entries"]}
    if len(by_address) != 786:
        raise ValueError("duplicate candidate address")
    entries = [checked_spec(by_address[address], bodies[address])
               for address in TARGETS]
    if [entry["address"] for entry in entries] != sorted(set(TARGETS)):
        raise ValueError("UI target order or uniqueness changed")
    manifest = {
        "schema_version": 1,
        "family": "instance_ui_initializer",
        "source": "out/function-inventory/registered-instance-candidates.json",
        "entry_count": len(entries),
        "registered_type_reference_count": sum(
            len(entry["registered_types"]) for entry in entries),
        "entries": entries,
    }
    write_if_changed(args.manifest, json.dumps(manifest, indent=2) + "\n")

    source = args.source.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ parameter table markers missing or duplicated")
    prefix, remainder = source.split(BEGIN, 1)
    _, suffix = remainder.split(END, 1)
    rows = "".join(
        f"    {{0x{entry['address'].lower()}u, "
        f"{entry['first_field_offset']}u, {entry['zero_field_offset']}u, "
        f"{entry['initial_field_word'].lower()}u, {entry['vtable'].lower()}u, "
        f"{entry['final_field_word'].lower()}u}},\n"
        for entry in entries)
    write_if_changed(args.source, prefix + BEGIN + rows + END + suffix)
    print(f"generated {len(entries)} exact UI instance initializers; "
          f"{manifest['registered_type_reference_count']} constructor references")


if __name__ == "__main__":
    main()
