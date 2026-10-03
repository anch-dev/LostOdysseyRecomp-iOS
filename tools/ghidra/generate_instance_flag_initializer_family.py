"""Validate and emit the four exact SceneCapture flag initializers."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED INSTANCE FLAG PARAMETERS\n"
END = "    // END GENERATED INSTANCE FLAG PARAMETERS\n"
TARGETS = ("8268C9E8", "8268CA78", "8268CB98", "8268CD08")


def checked_spec(entry: dict, body: list[str]) -> dict:
    address = entry["address"]
    ins = entry["instructions"]
    if len(ins) != 9 or ins[:4] != [
            "cmplwi cr6,r3,0", "beqlr cr6",
            "lwz r10,120(r3)", "lis r11,-32224"] or \
            ins[5:] != [
                "oris r10,r10,32768", "stw r11,0(r3)",
                "stw r10,120(r3)", "blr"]:
        raise ValueError(f"flag initializer skeleton changed: {address}")
    match = re.fullmatch(r"addi r11,r11,(-?\d+)", ins[4])
    if match is None:
        raise ValueError(f"flag initializer vtable immediate changed: {address}")
    lo = int(match.group(1))
    if entry["cfg_branches"] or entry["direct_calls"]:
        raise ValueError(f"flag initializer call/CFG changed: {address}")
    expected = [
        f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
        "PPC_FUNC_PROLOGUE();",
        f"// {ins[0]}",
        "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
        f"// {ins[1]}",
        "if (ctx.cr6.eq) return;",
        f"// {ins[2]}",
        "ctx.r10.u64 = PPC_LOAD_U32(ctx.r3.u32 + 120);",
        f"// {ins[3]}",
        "ctx.r11.s64 = -2111832064;",
        f"// {ins[4]}",
        f"ctx.r11.s64 = ctx.r11.s64 + {lo};",
        f"// {ins[5]}",
        "ctx.r10.u64 = ctx.r10.u64 | 2147483648;",
        f"// {ins[6]}",
        "PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r11.u32);",
        f"// {ins[7]}",
        "PPC_STORE_U32(ctx.r3.u32 + 120, ctx.r10.u32);",
        f"// {ins[8]}",
        "return;",
        "}",
    ]
    if body != expected:
        raise ValueError(f"full generated PPC body changed: {address}")
    return {
        "address": address,
        "vtable": signed_word(-32224, lo, address),
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
                        "LostOdysseyRecompSemantics/instance_flag_initializer_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/instance_flag_initializer_family.cpp")
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
        raise ValueError("flag target order or uniqueness changed")
    manifest = {
        "schema_version": 1,
        "family": "instance_flag_initializer",
        "source": "out/function-inventory/registered-instance-candidates.json",
        "entry_count": len(entries),
        "flag_offset": 120,
        "or_mask": "0x80000000",
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
    print(f"generated {len(entries)} exact flag instance initializers; "
          f"{manifest['registered_type_reference_count']} constructor references")


if __name__ == "__main__":
    main()
