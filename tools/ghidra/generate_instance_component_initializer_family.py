"""Validate and emit the exact 32-instruction component default family."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED INSTANCE COMPONENT PARAMETERS\n"
END = "    // END GENERATED INSTANCE COMPONENT PARAMETERS\n"
STORES = [
    ("stw", "r11", 96), ("stw", "r11", 368),
    ("stw", "r11", 372), ("stw", "r11", 376),
    ("std", "r11", 476),
    ("stfs", "f13", 496), ("stfs", "f0", 500),
    ("stw", "r10", 0),
    ("stfs", "f0", 504), ("stfs", "f0", 508),
    ("stfs", "f0", 512), ("stfs", "f13", 516),
    ("stfs", "f0", 520), ("stfs", "f0", 524),
    ("stfs", "f0", 528), ("stfs", "f0", 532),
    ("stfs", "f13", 536), ("stfs", "f0", 540),
    ("stfs", "f0", 544), ("stfs", "f0", 548),
    ("stfs", "f0", 552), ("stfs", "f13", 556),
]
PREFIX = [
    "cmplwi cr6,r3,0", "beqlr cr6",
    "lis r11,-32231", "lis r10,-32256",
    "lfs f13,-27252(r11)", "li r11,0",
    "lfs f0,3664(r10)",
]


def checked_spec(entry: dict, body: list[str]) -> dict | None:
    instructions = entry["instructions"]
    if len(instructions) != 32 or instructions[:7] != PREFIX or \
            instructions[-1] != "blr":
        return None
    hi_match = re.fullmatch(r"lis r10,(-?\d+)", instructions[7])
    lo_match = re.fullmatch(r"addi r10,r10,(-?\d+)", instructions[8])
    if hi_match is None or lo_match is None or instructions[9:-1] != [
            f"{opcode} {register},{offset}(r3)"
            for opcode, register, offset in STORES]:
        return None
    address = entry["address"]
    hi, lo = int(hi_match.group(1)), int(lo_match.group(1))
    vtable = signed_word(hi, lo, address)
    expected = [
        f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
        "PPC_FUNC_PROLOGUE();",
        "PPCRegister temp{};",
        f"// {instructions[0]}",
        "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
        f"// {instructions[1]}",
        "if (ctx.cr6.eq) return;",
        f"// {instructions[2]}",
        "ctx.r11.s64 = -2112290816;",
        f"// {instructions[3]}",
        "ctx.r10.s64 = -2113929216;",
        f"// {instructions[4]}",
        "ctx.fpscr.disableFlushMode();",
        "temp.u32 = PPC_LOAD_U32(ctx.r11.u32 + -27252);",
        "ctx.f13.f64 = double(temp.f32);",
        f"// {instructions[5]}",
        "ctx.r11.s64 = 0;",
        f"// {instructions[6]}",
        "temp.u32 = PPC_LOAD_U32(ctx.r10.u32 + 3664);",
        "ctx.f0.f64 = double(temp.f32);",
        f"// {instructions[7]}",
        f"ctx.r10.s64 = {hi << 16};",
        f"// {instructions[8]}",
        f"ctx.r10.s64 = ctx.r10.s64 + {lo};",
    ]
    for index, (opcode, register, offset) in enumerate(STORES, start=9):
        expected.append(f"// {instructions[index]}")
        if opcode == "stfs":
            expected += [f"temp.f32 = float(ctx.{register}.f64);",
                         f"PPC_STORE_U32(ctx.r3.u32 + {offset}, temp.u32);"]
        else:
            width = "U64" if opcode == "std" else "U32"
            member = "u64" if opcode == "std" else "u32"
            expected.append(
                f"PPC_STORE_{width}(ctx.r3.u32 + {offset}, ctx.{register}.{member});")
    expected += [f"// {instructions[31]}", "return;", "}"]
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
                        "LostOdysseyRecompSemantics/instance_component_initializer_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/instance_component_initializer_family.cpp")
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
        raise ValueError("component initializer count/uniqueness changed")
    manifest = {
        "schema_version": 1,
        "family": "instance_component_initializer",
        "source": "out/function-inventory/registered-instance-candidates.json",
        "entry_count": len(entries),
        "float_constant_addresses": ["0x8218958C", "0x82000E50"],
        "limitations": [
            "Finite inputs are compared with cached generated PPC; signaling-NaN "
            "word movement follows LoadedSingle's ISA mapping, which can differ "
            "from the generated C++ baseline, and has no hardware measurement.",
            "The original std zero store at object+476 is modeled as two word "
            "writes in ordinary bounded GuestMemory; fault/MMIO width is not covered.",
        ],
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
    print(f"generated {len(entries)} exact component instance initializers; "
          f"{manifest['registered_type_reference_count']} constructor references")


if __name__ == "__main__":
    main()
