"""Recover exact ordered integer field defaults from cached instance bodies."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
CACHE = ROOT / "out/function-inventory"
WRITES_BEGIN = "    // BEGIN GENERATED INSTANCE DEFAULT FIELD WRITES\n"
WRITES_END = "    // END GENERATED INSTANCE DEFAULT FIELD WRITES\n"
SPECS_BEGIN = "    // BEGIN GENERATED INSTANCE DEFAULT FIELD PARAMETERS\n"
SPECS_END = "    // END GENERATED INSTANCE DEFAULT FIELD PARAMETERS\n"
UI_IN_PROGRESS = {"82631AD0", "82631BE8", "826320A0", "82632250",
                  "82632480", "82632648"}
TARGETS = {
    "82412050", "82412338", "824123D8", "82412568", "8245D2B8",
    "82483228", "8248D208", "8249C470", "824C3648", "824D7128",
    "824D87E8", "824D9010", "824D90C0", "825AA370", "825F5E98",
    "826316C0", "82632130", "82632360", "8264E6A0", "82657250",
    "826576E0", "82657910", "8266CFB0", "8266D118", "8268CF20",
    "8268D090", "8268D260", "82698150", "8269CB00", "826B5E48",
    "826C0480", "826F7AA8", "826FA078", "826FA0D0", "82705AD8",
    "82706A60", "82706AF8", "8270F408", "82714A40", "82719FE8",
    "82720EF0", "8272F978",
}
REGISTER = r"r(?:[0-9]|[12][0-9]|3[01])"
CONSTANT_REGISTERS = {"r6", "r7", "r8", "r9", "r10", "r11"}
LIS = re.compile(rf"lis ({REGISTER}),(-?\d+)")
LI = re.compile(rf"li ({REGISTER}),(-?\d+)")
ADDI = re.compile(rf"addi ({REGISTER}),({REGISTER}),(-?\d+)")
STORE = re.compile(rf"(stw|stb) ({REGISTER}),(\d+)\(r3\)")


def signed16(value: str, address: str) -> int:
    number = int(value)
    if not -32768 <= number <= 32767:
        raise ValueError(f"signed immediate changed: {address}")
    return number


def recover(entry: dict, body: list[str]) -> dict | None:
    address = entry["address"]
    instructions = entry["instructions"]
    if instructions[:2] != ["cmplwi cr6,r3,0", "beqlr cr6"] or \
            instructions[-1] != "blr" or entry["cfg_branches"] or \
            entry["direct_calls"]:
        return None
    values: dict[str, int] = {}
    writes = []
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
                "PPC_FUNC_PROLOGUE();",
                "// cmplwi cr6,r3,0",
                "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
                "// beqlr cr6", "if (ctx.cr6.eq) return;"]
    for instruction in instructions[2:-1]:
        if match := LIS.fullmatch(instruction):
            register, literal = match.groups()
            if register not in CONSTANT_REGISTERS:
                return None
            immediate = signed16(literal, address)
            values[register] = (immediate << 16) & 0xFFFFFFFF
            translated = f"ctx.{register}.s64 = {immediate << 16};"
        elif match := LI.fullmatch(instruction):
            register, literal = match.groups()
            if register not in CONSTANT_REGISTERS:
                return None
            immediate = signed16(literal, address)
            values[register] = immediate & 0xFFFFFFFF
            translated = f"ctx.{register}.s64 = {immediate};"
        elif match := ADDI.fullmatch(instruction):
            register, source, literal = match.groups()
            if register not in CONSTANT_REGISTERS or \
                    source not in CONSTANT_REGISTERS or source not in values:
                return None
            immediate = signed16(literal, address)
            values[register] = (values[source] + immediate) & 0xFFFFFFFF
            translated = (f"ctx.{register}.s64 = ctx.{source}.s64 + "
                          f"{immediate};")
        elif match := STORE.fullmatch(instruction):
            operation, register, displacement = match.groups()
            offset = int(displacement)
            if register not in values or not 0 <= offset <= 0x7FFF:
                return None
            width = 8 if operation == "stb" else 32
            value = values[register] & (0xFF if width == 8 else 0xFFFFFFFF)
            writes.append({"offset": offset, "width_bits": width,
                           "value": f"0x{value:08X}"})
            suffix = "u8" if width == 8 else "u32"
            store = "PPC_STORE_U8" if width == 8 else "PPC_STORE_U32"
            translated = (f"{store}(ctx.r3.u32 + {offset}, "
                          f"ctx.{register}.{suffix});")
        else:
            return None
        expected.extend((f"// {instruction}", translated))
    expected += ["// blr", "return;", "}"]
    vtables = [write["value"] for write in writes
               if write["offset"] == 0 and write["width_bits"] == 32]
    if len(vtables) != 1 or not 1 <= len(writes) <= 255:
        return None
    if body != expected:
        raise ValueError(f"full translated body/CFG changed: {address}")
    return {"address": address, "vtable": vtables[0],
            "writes_in_order": writes,
            "source": entry["generated_ppc_path"],
            "source_line": entry["line"],
            "registered_types": entry["types"]}


def generate(candidates: dict, bodies: dict[str, list[str]]) -> dict:
    if len(candidates["entries"]) != 786 or len(bodies) != 786:
        raise ValueError("outgoing92 cache changed")
    known = {entry["address"] for path in SEMANTICS.glob("instance_*families.json")
             if path.name != "instance_default_fields_families.json"
             for entry in json.loads(path.read_text(encoding="utf-8"))["entries"]}
    if TARGETS & (known | UI_IN_PROGRESS):
        raise ValueError("default-field target overlaps another family")
    by_address = {entry["address"]: entry for entry in candidates["entries"]}
    if len(by_address) != 786:
        raise ValueError("duplicate outgoing92 cache address")
    selected = {}
    for entry in candidates["entries"]:
        address = entry["address"]
        if address in known | UI_IN_PROGRESS:
            continue
        spec = recover(entry, bodies[address]) if address in bodies else None
        if spec is not None:
            selected[address] = spec
    if set(selected) != TARGETS or len(TARGETS) != 42:
        missing = sorted(TARGETS - selected.keys())
        extra = sorted(selected.keys() - TARGETS)
        raise ValueError(f"reviewed default-field set changed: missing={missing}, extra={extra}")
    entries = [selected[address] for address in sorted(TARGETS)]
    if sum(len(entry["writes_in_order"]) for entry in entries) != 464 or \
            sum(write["width_bits"] == 8 for entry in entries
                for write in entry["writes_in_order"]) != 1:
        raise ValueError("default-field write inventory changed")
    return {"schema_version": 1, "family": "instance_default_fields",
            "source": "out/function-inventory/registered-instance-candidates.json",
            "entry_count": 42, "ordered_write_count": 464,
            "registered_type_reference_count": sum(len(entry["registered_types"])
                                                   for entry in entries),
            "entries": entries}


def replace_section(source: str, begin: str, end: str, rows: str) -> str:
    if source.count(begin) != 1 or source.count(end) != 1:
        raise ValueError("default-field C++ parameter markers changed")
    prefix, remainder = source.split(begin, 1)
    _, suffix = remainder.split(end, 1)
    return prefix + begin + rows + end + suffix


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    bodies = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    manifest = generate(candidates, bodies)
    manifest_path = SEMANTICS / "instance_default_fields_families.json"
    manifest_text = json.dumps(manifest, indent=2) + "\n"
    source_path = SEMANTICS / "src/instance_default_fields_family.cpp"
    source = source_path.read_text(encoding="utf-8")
    write_rows = []
    spec_rows = []
    first = 0
    for entry in manifest["entries"]:
        write_rows.append(f"    // {entry['address']}\n")
        for write in entry["writes_in_order"]:
            width = "Byte" if write["width_bits"] == 8 else "Word"
            write_rows.append(f"    {{{write['offset']}u, StoreWidth::{width}, "
                              f"{write['value'].lower()}u}},\n")
        count = len(entry["writes_in_order"])
        spec_rows.append(f"    {{0x{entry['address'].lower()}u, {first}u, "
                         f"{count}u}},\n")
        first += count
    source_text = replace_section(source, WRITES_BEGIN, WRITES_END,
                                  "".join(write_rows))
    source_text = replace_section(source_text, SPECS_BEGIN, SPECS_END,
                                  "".join(spec_rows))
    if args.check:
        if not manifest_path.exists() or manifest_path.read_text(encoding="utf-8") != manifest_text or \
                source != source_text:
            raise ValueError("default-field generated files differ from cache")
    else:
        write_if_changed(manifest_path, manifest_text)
        write_if_changed(source_path, source_text)
    print("validated 42 exact default-field bodies and 464 ordered writes")


if __name__ == "__main__":
    main()
