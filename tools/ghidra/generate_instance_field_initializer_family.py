"""Validate and emit the exact five-write instance field initializer family."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED INSTANCE FIELD INITIALIZER PARAMETERS\n"
END = "    // END GENERATED INSTANCE FIELD INITIALIZER PARAMETERS\n"
REGISTERS = (11, 10, 9, 8, 7)


def checked_spec(entry: dict, body: list[str]) -> dict | None:
    instructions = entry["instructions"]
    if len(instructions) != 18 or instructions[:2] != [
            "cmplwi cr6,r3,0", "beqlr cr6"] or instructions[-1] != "blr":
        return None
    hi = {}
    lo = {}
    for index, register in enumerate(REGISTERS):
        match = re.fullmatch(rf"lis r{register},(-?\d+)", instructions[2 + index])
        if match is None:
            return None
        hi[register] = int(match.group(1))
        match = re.fullmatch(
            rf"addi r{register},r{register},(-?\d+)", instructions[7 + index])
        if match is None:
            return None
        lo[register] = int(match.group(1))
    stores = []
    for index, register in enumerate(REGISTERS):
        match = re.fullmatch(rf"stw r{register},(\d+)\(r3\)",
                             instructions[12 + index])
        if match is None:
            return None
        stores.append(int(match.group(1)))
    first_offset = stores[0]
    if first_offset == 0 or first_offset > 0xffff or stores != [
            first_offset, first_offset + 4, 0,
            first_offset, first_offset + 4]:
        raise ValueError(f"five-write layout/order changed: {entry['address']}")
    if entry["cfg_branches"] or entry["direct_calls"]:
        raise ValueError(f"five-write control flow changed: {entry['address']}")

    address = entry["address"]
    expected = [
        f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
        "PPC_FUNC_PROLOGUE();",
        f"// {instructions[0]}",
        "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
        f"// {instructions[1]}",
        "if (ctx.cr6.eq) return;",
    ]
    for index, register in enumerate(REGISTERS):
        expected += [f"// {instructions[2 + index]}",
                     f"ctx.r{register}.s64 = {hi[register] << 16};"]
    for index, register in enumerate(REGISTERS):
        expected += [f"// {instructions[7 + index]}",
                     f"ctx.r{register}.s64 = ctx.r{register}.s64 + {lo[register]};"]
    for index, register in enumerate(REGISTERS):
        expected += [f"// {instructions[12 + index]}",
                     f"PPC_STORE_U32(ctx.r3.u32 + {stores[index]}, ctx.r{register}.u32);"]
    expected += [f"// {instructions[17]}", "return;", "}"]
    if body != expected:
        raise ValueError(f"full generated PPC body changed: {address}")
    words = {register: signed_word(hi[register], lo[register], address)
             for register in REGISTERS}
    return {
        "address": address,
        "first_field_offset": first_offset,
        "first_initial_word": words[11],
        "second_initial_word": words[10],
        "vtable": words[9],
        "first_final_word": words[8],
        "second_final_word": words[7],
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
                        "LostOdysseyRecompSemantics/instance_field_initializer_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/instance_field_initializer_family.cpp")
    args = parser.parse_args()

    candidates = json.loads(args.candidates.read_text(encoding="utf-8"))
    bodies = raw_bodies(args.originals)
    if len(bodies) != 786 or len(candidates["entries"]) != 786:
        raise ValueError("outgoing92 original inventory changed")
    entries = []
    for candidate in candidates["entries"]:
        address = candidate["address"]
        if address not in bodies:
            raise ValueError(f"missing original PPC body: {address}")
        spec = checked_spec(candidate, bodies[address])
        if spec is not None:
            entries.append(spec)
    entries.sort(key=lambda entry: entry["address"])
    if len(entries) != 19 or [entry["address"] for entry in entries] != \
            sorted({entry["address"] for entry in entries}):
        raise ValueError("five-write initializer family count/uniqueness changed")
    manifest = {
        "schema_version": 1,
        "family": "instance_field_initializer",
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
        f"    {{0x{entry['address'].lower()}u, {entry['first_field_offset']}u, "
        f"{entry['first_initial_word'].lower()}u, "
        f"{entry['second_initial_word'].lower()}u, {entry['vtable'].lower()}u, "
        f"{entry['first_final_word'].lower()}u, "
        f"{entry['second_final_word'].lower()}u}},\n"
        for entry in entries)
    write_if_changed(args.source, prefix + BEGIN + rows + END + suffix)
    print(f"generated {len(entries)} exact five-write instance initializers; "
          f"{manifest['registered_type_reference_count']} constructor references")


if __name__ == "__main__":
    main()
