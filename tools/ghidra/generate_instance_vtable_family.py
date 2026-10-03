"""Validate and emit the exact null-guarded instance vtable family."""

from __future__ import annotations

import argparse
import gzip
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED INSTANCE VTABLE PARAMETERS\n"
END = "    // END GENERATED INSTANCE VTABLE PARAMETERS\n"
SIX = [
    "cmplwi cr6,r3,0",
    "beqlr cr6",
    re.compile(r"lis r11,(-?\d+)"),
    re.compile(r"addi r11,r11,(-?\d+)"),
    "stw r11,0(r3)",
    "blr",
]
TWELVE = [
    "cmplwi cr6,r3,0",
    "beqlr cr6",
    re.compile(r"lis r11,(-?\d+)"),
    re.compile(r"lis r10,(-?\d+)"),
    re.compile(r"lis r9,(-?\d+)"),
    re.compile(r"addi r11,r11,(-?\d+)"),
    re.compile(r"addi r10,r10,(-?\d+)"),
    re.compile(r"addi r9,r9,(-?\d+)"),
    re.compile(r"stw r11,(\d+)\(r3\)"),
    "stw r10,0(r3)",
    re.compile(r"stw r9,(\d+)\(r3\)"),
    "blr",
]


def write_if_changed(path: Path, content: str) -> None:
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8", newline="\n")


def raw_bodies(path: Path) -> dict[str, list[str]]:
    raw = gzip.decompress(path.read_bytes()).decode("ascii")
    pieces = re.split(r"(?=PPC_FUNC_IMPL\(__imp__sub_[0-9A-F]{8}\) \{)", raw)
    bodies = {}
    for piece in pieces:
        if not piece.strip():
            continue
        lines = [line.strip() for line in piece.strip().splitlines()]
        match = re.fullmatch(r"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\) \{", lines[0])
        if not match or match.group(1) in bodies:
            raise ValueError("cached original body boundary or address changed")
        bodies[match.group(1)] = lines
    return bodies


def matches(instructions: list[str], pattern: list) -> bool:
    return len(instructions) == len(pattern) and all(
        (part.fullmatch(actual) is not None if isinstance(part, re.Pattern)
         else part == actual)
        for part, actual in zip(pattern, instructions))


def signed_word(hi: int, lo: int, address: str) -> str:
    if not (-32768 <= hi <= 32767 and -32768 <= lo <= 32767):
        raise ValueError(f"initializer immediate out of signed-16 range: {address}")
    return f"0x{((hi << 16) + lo) & 0xffffffff:08X}"


def checked_single_spec(entry: dict, body: list[str]) -> dict | None:
    instructions = entry["instructions"]
    if not matches(instructions, SIX):
        return None
    address = entry["address"]
    hi = int(SIX[2].fullmatch(instructions[2]).group(1))
    lo = int(SIX[3].fullmatch(instructions[3]).group(1))
    vtable = signed_word(hi, lo, address)
    expected = [
        f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
        "PPC_FUNC_PROLOGUE();",
        f"// {instructions[0]}",
        "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
        f"// {instructions[1]}",
        "if (ctx.cr6.eq) return;",
        f"// {instructions[2]}",
        f"ctx.r11.s64 = {hi << 16};",
        f"// {instructions[3]}",
        f"ctx.r11.s64 = ctx.r11.s64 + {lo};",
        f"// {instructions[4]}",
        "PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r11.u32);",
        f"// {instructions[5]}",
        "return;",
        "}",
    ]
    if body != expected or entry["cfg_branches"] or entry["direct_calls"]:
        raise ValueError(f"original PPC body/CFG no longer matches vtable initializer: {address}")
    return {
        "address": address,
        "kind": "vtable_only",
        "vtable": vtable,
        "field_offset": 0,
        "initial_field_word": "0x00000000",
        "final_field_word": "0x00000000",
        "source": entry["generated_ppc_path"],
        "source_line": entry["line"],
        "registered_types": entry["types"],
    }


def checked_ordered_spec(entry: dict, body: list[str]) -> dict | None:
    instructions = entry["instructions"]
    if not matches(instructions, TWELVE):
        return None
    address = entry["address"]
    hi = {register: int(TWELVE[index].fullmatch(instructions[index]).group(1))
          for register, index in ((11, 2), (10, 3), (9, 4))}
    lo = {register: int(TWELVE[index].fullmatch(instructions[index]).group(1))
          for register, index in ((11, 5), (10, 6), (9, 7))}
    first_offset = int(TWELVE[8].fullmatch(instructions[8]).group(1))
    last_offset = int(TWELVE[10].fullmatch(instructions[10]).group(1))
    if first_offset == 0 or first_offset != last_offset or first_offset > 0xffff:
        raise ValueError(f"ordered field offset changed: {address}")
    expected = [
        f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
        "PPC_FUNC_PROLOGUE();",
        f"// {instructions[0]}",
        "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
        f"// {instructions[1]}",
        "if (ctx.cr6.eq) return;",
    ]
    for register, index in ((11, 2), (10, 3), (9, 4)):
        expected += [f"// {instructions[index]}",
                     f"ctx.r{register}.s64 = {hi[register] << 16};"]
    for register, index in ((11, 5), (10, 6), (9, 7)):
        expected += [f"// {instructions[index]}",
                     f"ctx.r{register}.s64 = ctx.r{register}.s64 + {lo[register]};"]
    for register, index, offset in ((11, 8, first_offset),
                                    (10, 9, 0), (9, 10, last_offset)):
        expected += [f"// {instructions[index]}",
                     f"PPC_STORE_U32(ctx.r3.u32 + {offset}, ctx.r{register}.u32);"]
    expected += [f"// {instructions[11]}", "return;", "}"]
    if body != expected or entry["cfg_branches"] or entry["direct_calls"]:
        raise ValueError(f"original PPC body/CFG no longer matches ordered initializer: {address}")
    return {
        "address": address,
        "kind": "ordered_fields",
        "vtable": signed_word(hi[10], lo[10], address),
        "field_offset": first_offset,
        "initial_field_word": signed_word(hi[11], lo[11], address),
        "final_field_word": signed_word(hi[9], lo[9], address),
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
                        "LostOdysseyRecompSemantics/instance_vtable_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/instance_vtable_family.cpp")
    args = parser.parse_args()

    candidates = json.loads(args.candidates.read_text(encoding="utf-8"))
    bodies = raw_bodies(args.originals)
    if len(bodies) != candidates["candidate_count"] or len(bodies) != 786:
        raise ValueError("cached outgoing92 original inventory changed")
    entries = []
    for candidate in candidates["entries"]:
        address = candidate["address"]
        if address not in bodies:
            raise ValueError(f"missing original PPC body: {address}")
        spec = checked_single_spec(candidate, bodies[address])
        if spec is None:
            spec = checked_ordered_spec(candidate, bodies[address])
        if spec is not None:
            entries.append(spec)
    entries.sort(key=lambda entry: entry["address"])
    addresses = [entry["address"] for entry in entries]
    kinds = {kind: sum(entry["kind"] == kind for entry in entries)
             for kind in ("vtable_only", "ordered_fields")}
    if kinds != {"vtable_only": 550, "ordered_fields": 33} or \
            addresses != sorted(set(addresses)):
        raise ValueError("exact instance initializer count/uniqueness changed")
    manifest = {
        "schema_version": 1,
        "family": "instance_vtable_initializer",
        "source": "out/function-inventory/registered-instance-candidates.json",
        "entry_count": len(entries),
        "kind_counts": kinds,
        "registered_type_reference_count": sum(
            len(entry["registered_types"]) for entry in entries),
        "entries": entries,
    }
    write_if_changed(args.manifest, json.dumps(manifest, indent=2) + "\n")

    source = args.source.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ parameter table markers are missing or duplicated")
    prefix, remainder = source.split(BEGIN, 1)
    _, suffix = remainder.split(END, 1)
    rows = "".join(f"    {{0x{entry['address'].lower()}u, "
                   f"{entry['vtable'].lower()}u, {entry['field_offset']}u, "
                   f"{entry['initial_field_word'].lower()}u, "
                   f"{entry['final_field_word'].lower()}u}},\n" for entry in entries)
    write_if_changed(args.source, prefix + BEGIN + rows + END + suffix)
    print(f"generated {len(entries)} exact instance initializers "
          f"({kinds['vtable_only']} vtable-only, {kinds['ordered_fields']} ordered); "
          f"{manifest['registered_type_reference_count']} constructor references")


if __name__ == "__main__":
    main()
