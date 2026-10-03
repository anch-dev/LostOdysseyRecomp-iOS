"""Validate and emit the exact marker instance initializer family."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED INSTANCE MARKER PARAMETERS\n"
END = "    // END GENERATED INSTANCE MARKER PARAMETERS\n"


def checked_spec(entry: dict, body: list[str]) -> dict | None:
    instructions = entry["instructions"]
    if len(instructions) != 10:
        return None
    hi_match = re.fullmatch(r"lis r11,(-?\d+)", instructions[2])
    lo_match = re.fullmatch(r"addi r10,r11,(-?\d+)", instructions[3])
    if instructions[:2] != ["cmplwi cr6,r3,0", "beqlr cr6"] or \
            hi_match is None or lo_match is None or instructions[4:] != [
                "li r11,0", "stw r11,560(r3)", "stw r11,564(r3)",
                "stb r11,568(r3)", "stw r10,0(r3)", "blr"]:
        return None
    address = entry["address"]
    hi = int(hi_match.group(1))
    lo = int(lo_match.group(1))
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
        f"ctx.r10.s64 = ctx.r11.s64 + {lo};",
        f"// {instructions[4]}",
        "ctx.r11.s64 = 0;",
        f"// {instructions[5]}",
        "PPC_STORE_U32(ctx.r3.u32 + 560, ctx.r11.u32);",
        f"// {instructions[6]}",
        "PPC_STORE_U32(ctx.r3.u32 + 564, ctx.r11.u32);",
        f"// {instructions[7]}",
        "PPC_STORE_U8(ctx.r3.u32 + 568, ctx.r11.u8);",
        f"// {instructions[8]}",
        "PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r10.u32);",
        f"// {instructions[9]}",
        "return;",
        "}",
    ]
    if body != expected or entry["cfg_branches"] or entry["direct_calls"]:
        raise ValueError(f"full PPC body/CFG changed: {address}")
    return {
        "address": address,
        "vtable": vtable,
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
                        "LostOdysseyRecompSemantics/instance_marker_initializer_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/instance_marker_initializer_family.cpp")
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
    if len(entries) != 13 or [entry["address"] for entry in entries] != \
            sorted({entry["address"] for entry in entries}):
        raise ValueError("marker initializer count/uniqueness changed")
    manifest = {
        "schema_version": 1,
        "family": "instance_marker_initializer",
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
    rows = "".join(f"    {{0x{entry['address'].lower()}u, "
                   f"{entry['vtable'].lower()}u}},\n" for entry in entries)
    write_if_changed(args.source, prefix + BEGIN + rows + END + suffix)
    print(f"generated {len(entries)} exact marker instance initializers; "
          f"{manifest['registered_type_reference_count']} constructor references")


if __name__ == "__main__":
    main()
