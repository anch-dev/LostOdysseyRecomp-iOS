"""Recover fixed object/output and global writes, then compare one native PPC batch."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "out/function-inventory/remaining-memory-candidates.json"
MAP = ROOT / "LostOdysseyRecompSemantics/global_assignment_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/global_assignments_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/global_assignments.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-global-assignment-tests"
COUNTS = {
    "InitializeMultipleOutputFields": 5,
    "InitializeObjectAndGlobalFields": 12,
    "WriteGlobalConstantFields": 36,
    "WriteGlobalInputFields": 13,
    "WriteInputAndConstantFields": 22,
    "WriteInputFields": 15,
    "WriteObjectAndGlobalFields": 1,
}
REGISTERS = {f"r{i}" for i in range(3, 12)}
WIDTH = {8: "Byte", 16: "Halfword", 32: "Word"}
MASK = (1 << 64) - 1
SET = re.compile(r"ctx\.(r\d+)\.s64 = (-?\d+);")
ADD = re.compile(r"ctx\.(r\d+)\.s64 = ctx\.(r\d+)\.s64 \+ (-?\d+);")
OR = re.compile(r"ctx\.(r\d+)\.u64 = ctx\.(r\d+)\.u64 \| (\d+);")
STORE = re.compile(r"PPC_STORE_U(8|16|32)\(ctx\.(r\d+)\.u32 \+ (-?\d+), "
                   r"ctx\.(r\d+)\.u(8|16|32)\);")


def register(name: str) -> str:
    if name not in REGISTERS:
        raise ValueError(f"unsupported register: {name}")
    return "GlobalAssignmentRegister::R" + name[1:]


def fold_register(statement: str, known: dict[str, int]) -> tuple[str, int]:
    if match := SET.fullmatch(statement):
        destination, number = match.groups()
        result = int(number) & MASK
    elif match := ADD.fullmatch(statement):
        destination, source, number = match.groups()
        if source not in known:
            raise ValueError(f"non-constant addition source: {statement}")
        result = (known[source] + int(number)) & MASK
    elif match := OR.fullmatch(statement):
        destination, source, number = match.groups()
        if source not in known:
            raise ValueError(f"non-constant OR source: {statement}")
        result = known[source] | int(number)
    else:
        raise ValueError(f"unsupported register assignment: {statement}")
    register(destination)
    known[destination] = result
    return destination, result


def recover_entry(entry: dict) -> dict:
    address = entry["address"]
    if not re.fullmatch(r"[0-9A-F]{8}", address):
        raise ValueError(f"invalid address: {address}")
    effects = entry["instruction_effects"]
    sequence = entry["instruction_sequence"]
    if [effect["instruction"] for effect in effects] != sequence or sequence[-1] != "blr" or \
            effects[-1]["statements"] != ["return;"] or \
            any(len(effect["statements"]) != 1 for effect in effects) or \
            entry["generated_effect_statements"] != [e["statements"][0] for e in effects[:-1]] or \
            entry["generated_statements"] != ["PPC_FUNC_PROLOGUE();", *entry["generated_effect_statements"], "return;"]:
        raise ValueError(f"instruction effect mismatch: {address}")
    if entry["guest_context_side_effects"]["cr_writes"] or \
            entry["guest_context_side_effects"]["xer_writes"] or \
            entry["guest_context_side_effects"]["lr_writes"] or \
            entry["guest_context_side_effects"]["ctr_writes"]:
        raise ValueError(f"special register effect: {address}")

    stores = entry["parameters"]["ordered_stores"]
    if not stores:
        raise ValueError(f"empty assignment family: {address}")
    indexed_stores = {store["instruction_index"]: store for store in stores}
    if len(indexed_stores) != len(stores):
        raise ValueError(f"duplicate store index: {address}")
    known: dict[str, int] = {}
    before: list[dict] = []
    write_states: list[list[dict]] = []
    gpr_effects = []
    for index, effect in enumerate(effects[:-1]):
        statement = effect["statements"][0]
        if index not in indexed_stores:
            destination, value = fold_register(statement, known)
            before.append({"register": destination, "value_u64": value})
            gpr_effects.append((index, destination, statement))
            continue

        store = indexed_stores[index]
        match = STORE.fullmatch(statement)
        if match is None:
            raise ValueError(f"unrecognized field write: {address}:{index} {statement}")
        bits, base, displacement, source, source_bits = match.groups()
        bits = int(bits)
        if bits != int(source_bits) or bits not in WIDTH or bits != store["width"] or \
                source != store["source_register"]:
            raise ValueError(f"write width/source mismatch: {address}:{index}")
        ppc_store = re.fullmatch(r"(stb|sth|stw) (r\d+),(-?\d+)\((r\d+)\)", effect["instruction"])
        if ppc_store is None or ppc_store.groups() != \
                ({8: "stb", 16: "sth", 32: "stw"}[bits], source, displacement, base):
            raise ValueError(f"unexpected PPC write: {address}:{index}")

        location = store["address"]
        if location["kind"] == "global":
            if base not in known or (known[base] + int(displacement)) & 0xffffffff != location["address_u32"]:
                raise ValueError(f"global address mismatch: {address}:{index}")
        elif location["kind"] == "input_field":
            if base in known or location["base_register"] != base or \
                    location["displacement"] != int(displacement):
                raise ValueError(f"object/output address mismatch: {address}:{index}")
        else:
            raise ValueError(f"unsupported address kind: {address}:{index}")

        value = store["value"]
        if value["kind"] == "constant":
            if source not in known or known[source] & ((1 << bits) - 1) != value["value"]:
                raise ValueError(f"constant value mismatch: {address}:{index}")
        elif value["kind"] == "input_register":
            if source in known or value["register"] != source:
                raise ValueError(f"input value mismatch: {address}:{index}")
        else:
            raise ValueError(f"unsupported value kind: {address}:{index}")
        register(base)
        register(source)
        write_states.append(before)
        before = []

    listed_gpr_effects = [(item["instruction_index"], item["register"], item["statement"])
                          for item in entry["guest_context_side_effects"]["ordered_gpr_writes"]]
    if listed_gpr_effects != gpr_effects or \
            set(known) != set(entry["guest_context_side_effects"]["all_written_gprs"]) or \
            known != entry["parameters"]["final_constant_gprs"] or \
            sorted(indexed_stores) != [store["instruction_index"] for store in stores] or \
            len(gpr_effects) + len(stores) != len(effects) - 1:
        raise ValueError(f"effects or final registers mismatch: {address}")
    result = {key: value for key, value in entry.items()
              if key not in {"generated_statements", "status"}}
    result["semantic_plan"] = {"register_values_before_writes": write_states,
                               "register_values_after_writes": before}
    return result


def selected_entries() -> list[dict]:
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    if inventory["kind"] != "closed_named_semantic_family_candidates":
        raise ValueError("unexpected remaining-memory candidate schema")
    entries = [recover_entry(entry) for entry in inventory["entries"] if entry["family"] in COUNTS]
    counts = {family: sum(entry["family"] == family for entry in entries) for family in COUNTS}
    addresses = [entry["address"] for entry in entries]
    if counts != COUNTS or addresses != sorted(set(addresses)):
        raise ValueError(f"global assignment membership changed: {counts}")
    return entries


def check_originals(entries: list[dict], originals: bytes) -> None:
    source = "\n".join(line.rstrip(" \t") for line in
                       originals.decode("utf-8").replace("\r\n", "\n").splitlines())
    for entry in entries:
        expected = [f"PPC_FUNC_IMPL(__imp__sub_{entry['address']}) {{",
                    "\tPPC_FUNC_PROLOGUE();"]
        for effect in entry["instruction_effects"]:
            expected.extend((f"\t// {effect['instruction']}",
                             f"\t{effect['statements'][0]}"))
        expected.append("}")
        if "\n".join(expected) not in source:
            raise ValueError(f"generated PPC body changed: {entry['address']}")


def assignment_values(items: list[dict], name: str, lines: list[str]) -> str:
    if not items:
        return "{}"
    lines.append(f"static const ConstantRegisterValue {name}[] = {{")
    lines.extend(f"  {{{register(item['register'])}, 0x{item['value_u64']:016x}ull}},"
                 for item in items)
    lines.append("};")
    return name


def harness(entries: list[dict]) -> bytes:
    lines = [
        '#include "lo_semantics/global_assignments.h"',
        "using namespace lo::semantic::gpu;",
        "struct GlobalAssignmentEntry {",
        "  std::uint32_t address; void (*original)(PPCContext&, std::uint8_t*);",
        "  std::span<const AssignmentFieldWrite> writes;",
        "  std::span<const ConstantRegisterValue> register_values_after;",
        "};",
    ]
    table = []
    for entry in entries:
        address = entry["address"]
        writes = entry["parameters"]["ordered_stores"]
        phases = entry["semantic_plan"]["register_values_before_writes"]
        before_names = [assignment_values(values, f"kBefore{address}_{index}", lines)
                        for index, values in enumerate(phases)]
        after_name = assignment_values(entry["semantic_plan"]["register_values_after_writes"],
                                       f"kAfter{address}", lines)
        write_name = f"kWrites{address}"
        lines.append(f"static const AssignmentFieldWrite {write_name}[] = {{")
        for write, before_name in zip(writes, before_names, strict=True):
            location = write["address"]
            value = write["value"]
            is_global = location["kind"] == "global"
            is_constant = value["kind"] == "constant"
            fields = (
                "AssignmentAddressKind::Global" if is_global else "AssignmentAddressKind::ObjectField",
                register("r3" if is_global else location["base_register"]),
                f"0x{location['address_u32']:08x}u" if is_global else "0u",
                "0" if is_global else str(location["displacement"]),
                "AssignmentWidth::" + WIDTH[write["width"]],
                "AssignmentValueKind::Constant" if is_constant else "AssignmentValueKind::InputRegister",
                register("r3" if is_constant else value["register"]),
                f"0x{value['value']:08x}u" if is_constant else "0u",
                "{}" if before_name == "{}" else before_name,
            )
            lines.append("  {" + ", ".join(fields) + "},")
        lines.append("};")
        table.append(f"  {{0x{address}u, &__imp__sub_{address}, {write_name}, {after_name}}},")
    lines.append("static const GlobalAssignmentEntry kGlobalAssignmentEntries[] = {")
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
    mapping = {"schema_version": 1, "kind": "global_assignment_families",
               "family_counts": COUNTS, "entries": entries}
    if args.write_map:
        MAP.write_text(json.dumps(mapping, indent=2) + "\n", encoding="utf-8")
    elif not MAP.is_file() or json.loads(MAP.read_text(encoding="utf-8")) != mapping:
        raise ValueError("tracked global assignment map differs from reviewed candidates")
    originals = extract_originals([{"address": entry["address"],
                                    "generated_ppc_path": entry["source"],
                                    "line": entry["source_line"],
                                    "instruction_sequence": entry["instruction_sequence"]}
                                   for entry in entries], args.ppc_root)
    check_originals(entries, originals)
    result = compile_and_run("global-assignments", originals, harness(entries),
                             [SOURCE], args.output)
    expected = "PASS global-assignments 104 entries 416 cases"
    if result["summary"] != expected:
        raise ValueError(f"unexpected global assignment result: {result['summary']}")


if __name__ == "__main__":
    main()
