"""Recover 19 exact no-call scalar instance initializers from cached PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
CACHE = ROOT / "out/function-inventory"
BEGIN = "    // BEGIN GENERATED INSTANCE SCALAR PARAMETERS\n"
END = "    // END GENERATED INSTANCE SCALAR PARAMETERS\n"

GROUPS = {
    "zero72_before_vtable": {
        "824D5C80", "824D8BF8", "824D8CD0", "824D8DA8", "824D8E68",
        "824D8F30", "824D8FF0",
    },
    "four_zero_then_eight": {
        "8240AC40", "8256F938", "82599F30", "82603C78", "8266EB08",
        "8268CDC8", "826E2298",
    },
    "three_zero": {
        "8240AB00", "82411F48", "82483B20", "82656288", "82720E78",
    },
}
ENUMS = {"zero72_before_vtable": "ClearBeforeVtable",
         "four_zero_then_eight": "FourZerosAndEight",
         "three_zero": "ThreeZeros"}


def translated(instruction: str) -> str:
    if instruction == "cmplwi cr6,r3,0":
        return "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);"
    if instruction == "beqlr cr6":
        return "if (ctx.cr6.eq) return;"
    if instruction == "blr":
        return "return;"
    if match := re.fullmatch(r"lis (r\d+),(-?\d+)", instruction):
        register, immediate = match.groups()
        return f"ctx.{register}.s64 = {int(immediate) << 16};"
    if match := re.fullmatch(r"li (r\d+),(-?\d+)", instruction):
        register, immediate = match.groups()
        return f"ctx.{register}.s64 = {int(immediate)};"
    if match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction):
        target, source, immediate = match.groups()
        return f"ctx.{target}.s64 = ctx.{source}.s64 + {int(immediate)};"
    if match := re.fullmatch(r"stw (r\d+),(\d+)\(r3\)", instruction):
        register, offset = match.groups()
        return f"PPC_STORE_U32(ctx.r3.u32 + {int(offset)}, ctx.{register}.u32);"
    raise ValueError(f"unexpected scalar PPC instruction: {instruction}")


def recover(entry: dict, body: list[str], layout: str) -> dict:
    address = entry["address"]
    instructions = entry["instructions"]
    if instructions[:2] != ["cmplwi cr6,r3,0", "beqlr cr6"] or \
            instructions[-1] != "blr" or entry["cfg_branches"] or \
            entry["direct_calls"]:
        raise ValueError(f"scalar control flow changed: {address}")
    if layout == "zero72_before_vtable":
        if len(instructions) != 8:
            raise ValueError(f"clear-before-vtable shape changed: {address}")
        high = re.fullmatch(r"lis r11,(-?\d+)", instructions[2])
        low = re.fullmatch(r"addi r11,r11,(-?\d+)", instructions[4])
        first_field = 72
        expected = [*instructions[:2], instructions[2], "li r10,0",
                    instructions[4], "stw r10,72(r3)", "stw r11,0(r3)", "blr"]
    elif layout == "four_zero_then_eight":
        if len(instructions) != 13:
            raise ValueError(f"four-zero shape changed: {address}")
        high = re.fullmatch(r"lis r11,(-?\d+)", instructions[2])
        low = re.fullmatch(r"addi r10,r11,(-?\d+)", instructions[4])
        first = re.fullmatch(r"stw r11,(\d+)\(r3\)", instructions[7])
        first_field = int(first.group(1)) if first else -1
        expected = [*instructions[:2], instructions[2], "li r9,8",
                    instructions[4], "li r11,0", "stw r10,0(r3)",
                    *(f"stw r11,{first_field + 4 * index}(r3)" for index in range(4)),
                    f"stw r9,{first_field + 16}(r3)", "blr"]
    else:
        if len(instructions) != 10:
            raise ValueError(f"three-zero shape changed: {address}")
        high = re.fullmatch(r"lis r11,(-?\d+)", instructions[2])
        low = re.fullmatch(r"addi r10,r11,(-?\d+)", instructions[3])
        first = re.fullmatch(r"stw r11,(\d+)\(r3\)", instructions[6])
        first_field = int(first.group(1)) if first else -1
        expected = [*instructions[:2], instructions[2], instructions[3],
                    "li r11,0", "stw r10,0(r3)",
                    *(f"stw r11,{first_field + 4 * index}(r3)" for index in range(3)),
                    "blr"]
    if high is None or low is None or not 0 < first_field <= 0xFFF0 or \
            first_field % 4 != 0 or instructions != expected:
        raise ValueError(f"scalar instruction/order changed: {address}")
    expected_body = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
                     "PPC_FUNC_PROLOGUE();"]
    for instruction in instructions:
        expected_body.extend((f"// {instruction}", translated(instruction)))
    expected_body.append("}")
    if body != expected_body:
        raise ValueError(f"full translated scalar body changed: {address}")
    return {"address": address, "layout": layout,
            "vtable": signed_word(int(high.group(1)), int(low.group(1)), address),
            "first_field_offset": first_field,
            "source": entry["generated_ppc_path"], "source_line": entry["line"],
            "registered_types": entry["types"]}


def generate(candidates: dict, bodies: dict[str, list[str]]) -> dict:
    if len(candidates["entries"]) != 786 or len(bodies) != 786:
        raise ValueError("outgoing92 cached inventory changed")
    by_address = {entry["address"]: entry for entry in candidates["entries"]}
    targets = set().union(*GROUPS.values())
    if len(by_address) != 786 or sum(map(len, GROUPS.values())) != len(targets):
        raise ValueError("scalar target uniqueness changed")
    previous = {entry["address"] for path in SEMANTICS.glob("instance_*families.json")
                if path.name != "instance_scalar_initializer_families.json"
                for entry in json.loads(path.read_text(encoding="utf-8"))["entries"]}
    if targets & previous:
        raise ValueError("scalar targets overlap existing instance mapping")
    entries = []
    for layout, addresses in GROUPS.items():
        for address in addresses:
            if address not in by_address or address not in bodies:
                raise ValueError(f"scalar original missing: {address}")
            entries.append(recover(by_address[address], bodies[address], layout))
    entries.sort(key=lambda item: item["address"])
    return {"schema_version": 1, "family": "instance_scalar_initializer",
            "source": "out/function-inventory/registered-instance-candidates.json",
            "entry_count": len(entries),
            "registered_type_reference_count": sum(len(item["registered_types"])
                                                   for item in entries),
            "entries": entries}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    bodies = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    manifest = generate(candidates, bodies)
    manifest_path = SEMANTICS / "instance_scalar_initializer_families.json"
    manifest_text = json.dumps(manifest, indent=2) + "\n"
    source_path = SEMANTICS / "src/instance_scalar_initializer_family.cpp"
    source = source_path.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("scalar parameter table markers changed")
    prefix, remainder = source.split(BEGIN, 1)
    _, suffix = remainder.split(END, 1)
    rows = "".join(f"    {{0x{item['address'].lower()}u, "
                   f"{item['vtable'].lower()}u, {item['first_field_offset']}u, "
                   f"Layout::{ENUMS[item['layout']]}}},\n" for item in manifest["entries"])
    source_text = prefix + BEGIN + rows + END + suffix
    if args.check:
        if not manifest_path.exists() or manifest_path.read_text(encoding="utf-8") != manifest_text or \
                source != source_text:
            raise ValueError("scalar generated files differ from cached PPC")
    else:
        write_if_changed(manifest_path, manifest_text)
        write_if_changed(source_path, source_text)
    print(f"validated {len(manifest['entries'])} exact scalar initializers")


if __name__ == "__main__":
    main()
