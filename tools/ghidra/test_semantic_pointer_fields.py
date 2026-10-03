"""Compare reviewed constant-field and pointer-field operations with original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "out/function-inventory/next-memory-candidates.json"
MAP = ROOT / "LostOdysseyRecompSemantics/pointer_fields_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/pointer_fields_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/pointer_fields.cpp"
DEFAULT_PPC_ROOT = Path("C:/Users/freefrank/ownCloud/Git/LostOdysseyRecomp/LostOdysseyRecompLib/ppc")
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-pointer-fields-tests"
COUNTS = {
    "InitializeConstantFields": 227,
    "ReadPointerChainField": 35,
    "AddressOfPointerChainMember": 4,
    "ReadBaseByteIndexField": 3,
    "ReadBiasedIndexField": 6,
    "ReadPointerArrayElement": 2,
}
FAMILY = {
    "InitializeConstantFields": "Initialize",
    "ReadPointerChainField": "Chain",
    "AddressOfPointerChainMember": "Member",
    "ReadBaseByteIndexField": "ByteIndex",
    "ReadBiasedIndexField": "BiasedIndex",
    "ReadPointerArrayElement": "PointerArray",
}
WIDTH = {8: "Byte", 16: "Halfword", 32: "Word"}
VALID_REGISTERS = {"r3", "r4", "r5", "r6", "r9", "r10", "r11", "r13"}


def selected_entries() -> list[dict]:
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    if inventory["schema_version"] != 1 or inventory["kind"] != "next_memory_semantic_candidates_not_recovered":
        raise ValueError("unexpected next-memory candidate schema")
    entries = [entry for entry in inventory["entries"] if entry["family"] in COUNTS]
    counts = {family: sum(entry["family"] == family for entry in entries) for family in COUNTS}
    addresses = [entry["address"] for entry in entries]
    if counts != COUNTS or addresses != sorted(set(addresses)):
        raise ValueError(f"pointer-field membership changed: {counts}")
    for entry in entries:
        validate_entry(entry)
    return entries


def validate_entry(entry: dict) -> None:
    address = entry["address"]
    if not re.fullmatch(r"[0-9A-F]{8}", address):
        raise ValueError(f"invalid address: {address}")
    sequence = entry["instruction_sequence"]
    effects = entry["instruction_effects"]
    if sequence[-1] != "blr" or [part["instruction"] for part in effects] != sequence or \
            any(len(part["statements"]) != 1 for part in effects) or \
            effects[-1]["statements"] != ["return;"] or \
            entry["generated_effect_statements"] != [part["statements"][0] for part in effects[:-1]]:
        raise ValueError(f"instruction/effect contract changed: {address}")
    writes = entry["guest_context_side_effects"]["ordered_gpr_writes"]
    actual_writes = [(i, part["statements"][0]) for i, part in enumerate(effects[:-1])
                     if part["statements"][0].startswith("ctx.r")]
    if [(item["instruction_index"], item["statement"]) for item in writes] != actual_writes:
        raise ValueError(f"context write contract changed: {address}")
    if entry["guest_context_side_effects"]["special_register_effects"]:
        raise ValueError(f"unexpected special register effect: {address}")
    params = entry["parameters"]
    family = entry["family"]
    if family == "InitializeConstantFields":
        assignments = params["constant_register_assignments"]
        fields = params["ordered_writes"]
        if not assignments or not fields or params["base_register"] not in VALID_REGISTERS or \
                any(item["register"] not in VALID_REGISTERS or
                    item["register"] == params["base_register"] for item in assignments) or \
                any(item["base_register"] != params["base_register"] or
                    item["width"] not in WIDTH for item in fields) or \
                max(item["instruction_index"] for item in assignments) >= \
                min(item["instruction_index"] for item in fields):
            raise ValueError(f"initializer shape changed: {address}")
        indices = [item["instruction_index"] for item in assignments + fields]
        if sorted(indices) != list(range(len(sequence) - 1)):
            raise ValueError(f"initializer order changed: {address}")
    elif family in ("ReadPointerChainField", "AddressOfPointerChainMember"):
        loads = params["loads"]
        if len(loads) < 1 or any(load["width"] not in WIDTH for load in loads) or \
                loads[0]["base_register"] not in VALID_REGISTERS or \
                any(load["width"] != 32 or load["destination_register"] != "r11"
                    for load in loads[:-1]) or \
                any(load["base_register"] != "r11" for load in loads[1:]):
            raise ValueError(f"pointer-chain shape changed: {address}")
        if family == "ReadPointerChainField" and (len(loads) < 2 or
                                                   loads[-1]["destination_register"] != "r3"):
            raise ValueError(f"pointer-field shape changed: {address}")
        if family == "AddressOfPointerChainMember" and \
                (loads[-1]["width"] != 32 or loads[-1]["destination_register"] != "r11"):
            raise ValueError(f"pointer-member shape changed: {address}")
    elif family == "ReadBaseByteIndexField":
        expected = ("r3", "r4", "r11", 8)
        if tuple(params[key] for key in ("base_register", "byte_index_register",
                                         "address_register", "width")) != expected:
            raise ValueError(f"byte-index shape changed: {address}")
    elif family == "ReadBiasedIndexField":
        expected = ("r3", "r4", "r11", 4, 32)
        if tuple(params[key] for key in ("base_register", "index_register",
                                         "scratch_register", "element_stride", "width")) != expected:
            raise ValueError(f"biased-index shape changed: {address}")
    elif family == "ReadPointerArrayElement":
        expected = ("r3", "r4", "r11", "r10", 4, 32)
        if tuple(params[key] for key in ("object_register", "index_register",
                                         "pointer_register", "scaled_index_register",
                                         "element_stride", "width")) != expected:
            raise ValueError(f"pointer-array shape changed: {address}")


def check_originals(entries: list[dict], ppc_root: Path) -> None:
    files: dict[str, list[str]] = {}
    for entry in entries:
        name = Path(entry["source"]).name
        if not re.fullmatch(r"ppc_recomp\.\d+\.cpp", name):
            raise ValueError(f"unexpected source: {entry['source']}")
        if name not in files:
            files[name] = (ppc_root / name).read_text(encoding="utf-8").splitlines()
        expected = [f"PPC_FUNC_IMPL(__imp__sub_{entry['address']}) {{",
                    "\tPPC_FUNC_PROLOGUE();"]
        for effect in entry["instruction_effects"]:
            expected.append(f"\t// {effect['instruction']}")
            expected.append(f"\t{effect['statements'][0]}")
        expected.append("}")
        start = entry["source_line"] - 1
        if [line.rstrip(" \t") for line in files[name][start:start + len(expected)]] != expected:
            raise ValueError(f"generated PPC body changed: {entry['address']} {name}:{entry['source_line']}")


def reg(name: str) -> str:
    if name not in VALID_REGISTERS:
        raise ValueError(f"unreviewed register: {name}")
    return "PointerFieldRegister::R" + name[1:]


def width(bits: int) -> str:
    return "PointerFieldWidth::" + WIDTH[bits]


def harness(entries: list[dict]) -> bytes:
    lines = [
        '#include "lo_semantics/pointer_fields.h"',
        'using namespace lo::semantic::gpu;',
        'enum class PointerFieldFamily { Initialize, Chain, Member, ByteIndex, BiasedIndex, PointerArray };',
        'struct PointerFieldEntry {',
        '  std::uint32_t address; void (*original)(PPCContext&, std::uint8_t*);',
        '  PointerFieldFamily family; PointerFieldRegister base_register;',
        '  const ConstantFieldAssignment* assignments; std::size_t assignment_count;',
        '  const ConstantFieldWrite* writes; std::size_t write_count;',
        '  const PointerFieldOffset* loads; std::size_t load_count;',
        '  std::int32_t displacement;',
        '};',
    ]
    table = []
    for entry in entries:
        address = entry["address"]
        params = entry["parameters"]
        family = entry["family"]
        base = "r3"
        assignment_ref, assignment_count = "nullptr", 0
        write_ref, write_count = "nullptr", 0
        load_ref, load_count = "nullptr", 0
        displacement = 0
        if family == "InitializeConstantFields":
            base = params["base_register"]
            assignments = params["constant_register_assignments"]
            fields = params["ordered_writes"]
            assignment_ref, assignment_count = f"kAssignments{address}", len(assignments)
            write_ref, write_count = f"kWrites{address}", len(fields)
            lines.append(f"static const ConstantFieldAssignment {assignment_ref}[] = {{")
            lines.extend(f"  {{{reg(item['register'])}, 0x{item['value_u64']:016x}ull}},"
                         for item in assignments)
            lines.append("};")
            lines.append(f"static const ConstantFieldWrite {write_ref}[] = {{")
            lines.extend(f"  {{{item['displacement']}, {width(item['width'])}, 0x{item['value']:08x}u}},"
                         for item in fields)
            lines.append("};")
        elif family in ("ReadPointerChainField", "AddressOfPointerChainMember"):
            loads = params["loads"]
            base = loads[0]["base_register"]
            load_ref, load_count = f"kLoads{address}", len(loads)
            lines.append(f"static const PointerFieldOffset {load_ref}[] = {{")
            lines.extend(f"  {{{item['displacement']}, {width(item['width'])}}},"
                         for item in loads)
            lines.append("};")
            if family == "AddressOfPointerChainMember":
                displacement = params["member_displacement"]
        elif family == "ReadBaseByteIndexField":
            displacement = params["displacement"]
        elif family == "ReadBiasedIndexField":
            displacement = params["index_bias"]
        elif family == "ReadPointerArrayElement":
            displacement = params["array_pointer_displacement"]
        table.append(
            f"  {{0x{address}u, &__imp__sub_{address}, PointerFieldFamily::{FAMILY[family]}, "
            f"{reg(base)}, {assignment_ref}, {assignment_count}, {write_ref}, {write_count}, "
            f"{load_ref}, {load_count}, {displacement}}},"
        )
    lines.append("static const PointerFieldEntry kPointerFieldEntries[] = {")
    lines.extend(table)
    lines.append("};")
    return ("\n".join(lines) + "\n").encode() + ORACLE.read_bytes()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ppc-root", type=Path, default=DEFAULT_PPC_ROOT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--write-map", action="store_true")
    args = parser.parse_args()
    entries = selected_entries()
    mapping = {"schema_version": 1, "kind": "pointer_fields_families",
               "family_counts": COUNTS, "entries": entries}
    if args.write_map:
        MAP.write_text(json.dumps(mapping, indent=2) + "\n", encoding="utf-8")
    elif not MAP.is_file() or json.loads(MAP.read_text(encoding="utf-8")) != mapping:
        raise ValueError("tracked pointer-field map differs from strict candidates")
    check_originals(entries, args.ppc_root)
    original_entries = [{"address": entry["address"],
                         "generated_ppc_path": entry["source"],
                         "line": entry["source_line"],
                         "instruction_sequence": entry["instruction_sequence"]}
                        for entry in entries]
    originals = extract_originals(original_entries, args.ppc_root)
    result = compile_and_run("pointer-fields", originals, harness(entries), [SOURCE], args.output)
    expected = "PASS pointer-fields 277 entries 2216 cases"
    if result["summary"] != expected:
        raise ValueError(f"unexpected pointer-field result: {result['summary']}")


if __name__ == "__main__":
    main()
