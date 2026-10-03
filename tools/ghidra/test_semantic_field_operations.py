"""Compare reviewed field-copy, global-read, and predicate families with original PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from semantic_batch import compile_and_run, extract_originals


ROOT = Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "out/function-inventory/remaining-memory-candidates.json"
MAP = ROOT / "LostOdysseyRecompSemantics/field_operation_families.json"
ORACLE = ROOT / "LostOdysseyRecompSemantics/tests/field_operations_oracle.cpp"
SOURCE = ROOT / "LostOdysseyRecompSemantics/src/field_operations.cpp"
DEFAULT_PPC_ROOT = ROOT / "LostOdysseyRecompLib/ppc"
DEFAULT_OUTPUT = Path.home() / "worktrees/LostOdysseyRecomp/semantic-field-operation-tests"
COUNTS = {
    "CopyFixedFields": 57,
    "CopyFixedFieldsAndReturnConstant": 13,
    "ReadGlobalField": 33,
    "ReadGlobalPointerChainField": 5,
    "FieldEqualsConstant": 15,
    "FieldNotEqualsConstant": 11,
}
REGISTER = {f"r{i}" for i in range(3, 12)}
OFFSET = re.compile(r"ctx\.(r\d+)\.s64 = ctx\.(r\d+)\.s64 \+ (-?\d+);")
READ = re.compile(r"ctx\.(r\d+)\.u64 = PPC_LOAD_U(8|16|32)\(ctx\.(r\d+)\.u32 \+ (-?\d+)\);")
WRITE = re.compile(r"PPC_STORE_U(8|16|32)\(ctx\.(r\d+)\.u32 \+ (-?\d+), ctx\.(r\d+)\.u(8|16|32)\);")
RETURN = re.compile(r"ctx\.r3\.s64 = (-?\d+);")


def reg(name: str) -> str:
    if name not in REGISTER:
        raise ValueError(f"unsupported register: {name}")
    return "FieldOperationRegister::R" + name[1:]


def checked_effects(entry: dict) -> list[str]:
    address = entry["address"]
    if not re.fullmatch(r"[0-9A-F]{8}", address):
        raise ValueError(f"invalid address: {address}")
    sequence = entry["instruction_sequence"]
    effects = entry["instruction_effects"]
    if sequence[-1] != "blr" or [item["instruction"] for item in effects] != sequence or \
            any(len(item["statements"]) != 1 for item in effects) or \
            effects[-1]["statements"] != ["return;"]:
        raise ValueError(f"instruction effects changed: {address}")
    statements = [item["statements"][0] for item in effects[:-1]]
    if statements != entry["generated_effect_statements"] or \
            entry["generated_statements"] != ["PPC_FUNC_PROLOGUE();", *statements, "return;"]:
        raise ValueError(f"generated statements changed: {address}")
    context = entry["guest_context_side_effects"]
    writes = [(i, stmt) for i, stmt in enumerate(statements) if stmt.startswith("ctx.")]
    if [(item["instruction_index"], item["statement"]) for item in context["ordered_gpr_writes"]] != writes or \
            any(context[key] for key in ("cr_writes", "xer_writes", "lr_writes", "ctr_writes")):
        raise ValueError(f"guest context contract changed: {address}")
    return statements


def copy_steps(entry: dict, statements: list[str]) -> list[tuple]:
    steps = []
    actual_memory = []
    constants = []
    for index, statement in enumerate(statements):
        if match := OFFSET.fullmatch(statement):
            destination, source, displacement = match.groups()
            steps.append(("AddressBase", destination, source, int(displacement), 0, 0))
        elif match := READ.fullmatch(statement):
            destination, width, source, displacement = match.groups()
            steps.append(("Read", destination, source, int(displacement), int(width), 0))
            actual_memory.append((index, "read", int(width), destination))
        elif match := WRITE.fullmatch(statement):
            width, destination, displacement, source, source_width = match.groups()
            if width != source_width:
                raise ValueError(f"store width changed: {entry['address']}")
            steps.append(("Write", destination, source, int(displacement), int(width), 0))
            actual_memory.append((index, "write", int(width), source))
        elif match := RETURN.fullmatch(statement):
            value = int(match[1])
            steps.append(("ReturnConstant", "r3", "r3", 0, 0, value))
            constants.append(value)
        else:
            raise ValueError(f"unmodeled field copy effect: {entry['address']} {statement}")
    params = entry["parameters"]
    expected_memory = [(item["instruction_index"], item["kind"], item["width"],
                        item["destination_register"] if item["kind"] == "read" else item["source_register"])
                       for item in params["ordered_memory_operations"]]
    if actual_memory != expected_memory or len(actual_memory) < 2 or \
            len([s for s in steps if s[0] == "Read"]) != \
            len([s for s in steps if s[0] == "Write"]):
        raise ValueError(f"copy memory order changed: {entry['address']}")
    expected_return = params["constant_return_u64"]
    if constants != ([] if expected_return is None else [expected_return]) or \
            (entry["family"] == "CopyFixedFields") != (expected_return is None):
        raise ValueError(f"copy return changed: {entry['address']}")
    for kind, destination, source, _, width, _ in steps:
        reg(destination)
        reg(source)
        if kind in ("Read", "Write") and width not in (8, 32):
            raise ValueError(f"copy width changed: {entry['address']}")
    return steps


def global_loads(entry: dict, statements: list[str]) -> tuple[int, int | None, list[tuple]]:
    params = entry["parameters"]
    base = params["global_base_u32"]
    if not 0 <= base <= 0xffffffff:
        raise ValueError(f"global base changed: {entry['address']}")
    match = re.fullmatch(r"ctx\.r11\.s64 = (-?\d+);", statements[0])
    initial = int(match[1]) if match else 0
    if not match or initial & 0xffff or not -(1 << 31) <= initial <= (1 << 31) - 1:
        raise ValueError(f"global lis changed: {entry['address']}")
    cursor = 1
    adjustment = None
    if cursor < len(statements) and (match := OFFSET.fullmatch(statements[cursor])):
        if match[1] != "r11" or match[2] != "r11" or entry["family"] != "ReadGlobalField":
            raise ValueError(f"global adjustment changed: {entry['address']}")
        adjustment = int(match[3])
        cursor += 1
    if (initial + (adjustment or 0)) & 0xffffffff != base:
        raise ValueError(f"global base/adjustment changed: {entry['address']}")
    loads = []
    for item in params["ordered_loads"]:
        if item["instruction_index"] != cursor or item["base_register"] != "r11":
            raise ValueError(f"global read order changed: {entry['address']}")
        match = READ.fullmatch(statements[cursor]) if cursor < len(statements) else None
        if not match or (match[1], int(match[2]), match[3], int(match[4])) != \
                (item["destination_register"], item["width"], "r11", item["displacement"]):
            raise ValueError(f"global load changed: {entry['address']}")
        loads.append((item["destination_register"], item["displacement"], item["width"]))
        cursor += 1
    if cursor != len(statements) or not loads or loads[-1][0] != "r3" or \
            any(load[0] != "r11" or load[2] != 32 for load in loads[:-1]) or \
            (entry["family"] == "ReadGlobalField") != (len(loads) == 1):
        raise ValueError(f"global read shape changed: {entry['address']}")
    first = (base + loads[0][1]) & 0xffffffff
    if first != params["first_field_address_u32"]:
        raise ValueError(f"global address changed: {entry['address']}")
    return initial & 0xffffffff, adjustment, loads


def predicate(entry: dict, statements: list[str]) -> None:
    params = entry["parameters"]
    width, base, displacement, comparison = (params[key] for key in
        ("width", "base_register", "displacement", "comparison_u32"))
    if width not in (8, 32) or comparison < 0 or comparison > 0xffffffff:
        raise ValueError(f"predicate parameters changed: {entry['address']}")
    reg(base)
    expected = [f"ctx.r11.u64 = PPC_LOAD_U{width}(ctx.{base}.u32 + {displacement});"]
    if comparison:
        expected.append(f"ctx.r11.s64 = ctx.r11.s64 + {-comparison};")
    expected.append("ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);")
    rotate = "__builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;"
    if entry["family"] == "FieldEqualsConstant":
        expected.append("ctx.r3.u64 = " + rotate)
    else:
        expected.extend(("ctx.r11.u64 = " + rotate, "ctx.r3.u64 = ctx.r11.u64 ^ 1;"))
    if statements != expected:
        raise ValueError(f"predicate effect changed: {entry['address']}")


def selected_entries() -> list[dict]:
    inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
    if inventory["kind"] != "closed_named_semantic_family_candidates" or \
            inventory["candidate_count"] != 238:
        raise ValueError("unexpected candidate inventory")
    entries = [entry for entry in inventory["entries"] if entry["family"] in COUNTS]
    counts = {family: sum(entry["family"] == family for entry in entries) for family in COUNTS}
    addresses = [entry["address"] for entry in entries]
    if counts != COUNTS or addresses != sorted(set(addresses)):
        raise ValueError(f"field operation membership changed: {counts}")
    for entry in entries:
        statements = checked_effects(entry)
        if entry["family"].startswith("Copy"):
            copy_steps(entry, statements)
        elif entry["family"].startswith("ReadGlobal"):
            global_loads(entry, statements)
        else:
            predicate(entry, statements)
    return entries


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
            expected.extend((f"\t// {effect['instruction']}", f"\t{effect['statements'][0]}"))
        expected.append("}")
        start = entry["source_line"] - 1
        if [line.rstrip(" \t") for line in files[name][start:start + len(expected)]] != expected:
            raise ValueError(f"original PPC body changed: {entry['address']} {name}:{entry['source_line']}")


def harness(entries: list[dict]) -> bytes:
    lines = [
        '#include "lo_semantics/field_operations.h"',
        'using namespace lo::semantic::gpu;',
        'enum class Family { Copy, CopyReturn, Global, GlobalChain, Equals, NotEquals };',
        'struct Entry {',
        '  std::uint32_t address; void (*original)(PPCContext&, std::uint8_t*); Family family;',
        '  const FieldCopyStep* steps; std::size_t step_count;',
        '  std::uint32_t global_base; std::int32_t adjustment; bool has_adjustment;',
        '  const GlobalFieldLoad* loads; std::size_t load_count; std::uint32_t first_global_address;',
        '  FieldOperationRegister base; std::int32_t displacement; std::uint8_t width;',
        '  std::uint32_t comparison;',
        '};',
    ]
    families = {"CopyFixedFields": "Copy", "CopyFixedFieldsAndReturnConstant": "CopyReturn",
                "ReadGlobalField": "Global", "ReadGlobalPointerChainField": "GlobalChain",
                "FieldEqualsConstant": "Equals", "FieldNotEqualsConstant": "NotEquals"}
    table = []
    for entry in entries:
        address = entry["address"]
        family = entry["family"]
        params = entry["parameters"]
        step_ref, step_count = "nullptr", 0
        load_ref, load_count = "nullptr", 0
        global_base, adjustment, has_adjustment, first = 0, 0, "false", 0
        base, displacement, width, comparison = "r3", 0, 0, 0
        if family.startswith("Copy"):
            steps = copy_steps(entry, entry["generated_effect_statements"])
            step_ref, step_count = f"kSteps{address}", len(steps)
            lines.append(f"static const FieldCopyStep {step_ref}[] = {{")
            lines.extend(f"  {{FieldCopyKind::{kind}, {reg(destination)}, {reg(source)}, "
                         f"{offset}, {bits}, {constant}ull}},"
                         for kind, destination, source, offset, bits, constant in steps)
            lines.append("};")
        elif family.startswith("ReadGlobal"):
            initial_base, adjustment_value, loads = global_loads(entry, entry["generated_effect_statements"])
            load_ref, load_count = f"kLoads{address}", len(loads)
            lines.append(f"static const GlobalFieldLoad {load_ref}[] = {{")
            lines.extend(f"  {{{reg(destination)}, {offset}, {bits}}},"
                         for destination, offset, bits in loads)
            lines.append("};")
            global_base = initial_base
            first = params["first_field_address_u32"]
            if adjustment_value is not None:
                adjustment, has_adjustment = adjustment_value, "true"
        else:
            base = params["base_register"]
            displacement = params["displacement"]
            width = params["width"]
            comparison = params["comparison_u32"]
        table.append(
            f"  {{0x{address}u, &__imp__sub_{address}, Family::{families[family]}, "
            f"{step_ref}, {step_count}, 0x{global_base:08x}u, {adjustment}, {has_adjustment}, "
            f"{load_ref}, {load_count}, 0x{first:08x}u, {reg(base)}, {displacement}, "
            f"{width}, {comparison}u}},"
        )
    lines.append("static const Entry kEntries[] = {")
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
    mapping = {"schema_version": 1, "kind": "field_operation_families",
               "family_counts": COUNTS,
               "entries": [{key: value for key, value in entry.items() if key != "status"}
                           for entry in entries]}
    if args.write_map:
        MAP.write_text(json.dumps(mapping, indent=2) + "\n", encoding="utf-8")
    elif not MAP.is_file() or json.loads(MAP.read_text(encoding="utf-8")) != mapping:
        raise ValueError("tracked field operation map differs from strict candidates")
    check_originals(entries, args.ppc_root)
    original_entries = [{"address": entry["address"],
                         "generated_ppc_path": entry["source"],
                         "line": entry["source_line"],
                         "instruction_sequence": entry["instruction_sequence"]}
                        for entry in entries]
    originals = extract_originals(original_entries, args.ppc_root)
    result = compile_and_run("field-operations", originals, harness(entries), [SOURCE], args.output)
    expected = "PASS field-operations 134 entries 536 cases"
    if result["summary"] != expected:
        raise ValueError(f"unexpected field-operation result: {result['summary']}")


if __name__ == "__main__":
    main()
