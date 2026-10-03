"""Generate opt-in native PPC wrappers for reviewed object-memory families."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


FAMILY_KIND = {
    "pointer_fields": "pointer_fields_families",
    "global_assignments": "global_assignment_families",
    "field_operations": "field_operation_families",
}
REGISTERS = {
    "pointer_fields": (3, 4, 5, 6, 9, 10, 11, 13),
    "global_assignments": tuple(range(3, 12)),
    "field_operations": tuple(range(3, 12)),
}
COPY_OFFSET = re.compile(r"ctx\.(r\d+)\.s64 = ctx\.(r\d+)\.s64 \+ (-?\d+);")
COPY_READ = re.compile(r"ctx\.(r\d+)\.u64 = PPC_LOAD_U(8|16|32)\(ctx\.(r\d+)\.u32 \+ (-?\d+)\);")
COPY_WRITE = re.compile(r"PPC_STORE_U(8|16|32)\(ctx\.(r\d+)\.u32 \+ (-?\d+), ctx\.(r\d+)\.u(8|16|32)\);")
COPY_RETURN = re.compile(r"ctx\.r3\.s64 = (-?\d+);")


def checked_entries(manifest: dict, family: str) -> list[dict]:
    if family not in FAMILY_KIND or manifest.get("schema_version") != 1 or \
            manifest.get("kind") != FAMILY_KIND[family]:
        raise ValueError(f"unsupported {family} manifest")
    entries = manifest.get("entries")
    counts = manifest.get("family_counts")
    if not isinstance(entries, list) or not entries or not isinstance(counts, dict):
        raise ValueError(f"empty or incomplete {family} manifest")
    addresses = [entry.get("address") for entry in entries]
    if any(not isinstance(address, str) or not re.fullmatch(r"[0-9A-F]{8}", address)
           for address in addresses) or addresses != sorted(set(addresses)):
        raise ValueError(f"invalid or duplicate {family} address")
    if sum(counts.values()) != len(entries) or any(
            sum(entry.get("family") == name for entry in entries) != count
            for name, count in counts.items()):
        raise ValueError(f"{family} family counts changed")
    for entry in entries:
        effects = entry.get("instruction_effects")
        if not isinstance(effects, list) or not effects or \
                [effect["instruction"] for effect in effects] != entry.get("instruction_sequence") or \
                effects[-1]["statements"] != ["return;"] or \
                any(len(effect["statements"]) != 1 for effect in effects) or \
                [effect["statements"][0] for effect in effects[:-1]] != entry.get("generated_effect_statements"):
            raise ValueError(f"unreviewed instruction effects at {entry['address']}")
        if entry["family"] not in counts:
            raise ValueError(f"unreviewed family at {entry['address']}")
    return entries


def register(name: str, family: str) -> str:
    if not isinstance(name, str) or not re.fullmatch(r"r\d+", name) or \
            int(name[1:]) not in REGISTERS[family]:
        raise ValueError(f"unreviewed register {name}")
    return {
        "pointer_fields": "PointerFieldRegister",
        "global_assignments": "GlobalAssignmentRegister",
        "field_operations": "FieldOperationRegister",
    }[family] + "::R" + name[1:]


def capture_registers(family: str) -> list[str]:
    kind = {
        "pointer_fields": "PointerFieldRegisters",
        "global_assignments": "GlobalAssignmentRegisters",
        "field_operations": "FieldOperationRegisters",
    }[family]
    members = ", ".join(f".r{n} = ctx.r{n}.u64" for n in REGISTERS[family])
    return [f"    lo::semantic::gpu::{kind} registers{{{members}}};"]


def restore_registers(family: str) -> list[str]:
    return [f"    ctx.r{n}.u64 = registers.r{n};" for n in REGISTERS[family]]


def pointer_operation(entry: dict) -> tuple[list[str], str]:
    address, family, params = entry["address"], entry["family"], entry["parameters"]
    lines: list[str] = []
    prefix = "lo::semantic::gpu::"
    if family == "InitializeConstantFields":
        assignments = params["constant_register_assignments"]
        writes = params["ordered_writes"]
        if not assignments or not writes:
            raise ValueError(f"empty pointer initializer {address}")
        lines.append("    static const ConstantFieldAssignment assignments[] = {")
        lines.extend(f"        {{{register(item['register'], 'pointer_fields')}, 0x{item['value_u64']:016x}ull}},"
                     for item in assignments)
        lines.append("    };")
        lines.append("    static const ConstantFieldWrite writes[] = {")
        for item in writes:
            width = {8: "Byte", 16: "Halfword", 32: "Word"}.get(item["width"])
            if width is None:
                raise ValueError(f"unsupported pointer write width {address}")
            lines.append(f"        {{{item['displacement']}, PointerFieldWidth::{width}, 0x{item['value']:08x}u}},")
        lines.append("    };")
        call = (f"{prefix}InitializeConstantFieldsWith(memory, registers, "
                f"{register(params['base_register'], 'pointer_fields')}, assignments, writes);")
    elif family in ("ReadPointerChainField", "AddressOfPointerChainMember"):
        loads = params["loads"]
        if not loads:
            raise ValueError(f"empty pointer chain {address}")
        lines.append("    static const PointerFieldOffset loads[] = {")
        for item in loads:
            width = {8: "Byte", 16: "Halfword", 32: "Word"}.get(item["width"])
            if width is None:
                raise ValueError(f"unsupported pointer load width {address}")
            lines.append(f"        {{{item['displacement']}, PointerFieldWidth::{width}}},")
        lines.append("    };")
        root = register(loads[0]["base_register"], "pointer_fields")
        if family == "ReadPointerChainField":
            call = f"{prefix}ReadPointerChainFieldWith(memory, registers, {root}, loads);"
        else:
            call = (f"{prefix}AddressOfPointerChainMemberWith(memory, registers, {root}, "
                    f"loads, {params['member_displacement']});")
    else:
        key = {
            "ReadBaseByteIndexField": "displacement",
            "ReadBiasedIndexField": "index_bias",
            "ReadPointerArrayElement": "array_pointer_displacement",
        }.get(family)
        if key is None:
            raise ValueError(f"unreviewed pointer operation {address}")
        call = f"{prefix}{family}With(memory, registers, {params[key]});"
    return lines, call


def global_operation(entry: dict) -> tuple[list[str], str]:
    address = entry["address"]
    params = entry["parameters"]
    plan = entry["semantic_plan"]
    writes = params["ordered_stores"]
    before = plan["register_values_before_writes"]
    if not writes or len(writes) != len(before):
        raise ValueError(f"incomplete assignment phases at {address}")
    lines: list[str] = []

    def emit_values(values: list[dict], name: str) -> str:
        if not values:
            return "{}"
        lines.append(f"    static const ConstantRegisterValue {name}[] = {{")
        lines.extend(f"        {{{register(item['register'], 'global_assignments')}, "
                     f"0x{item['value_u64']:016x}ull}}," for item in values)
        lines.append("    };")
        return name

    before_names = [emit_values(values, f"before_{index}") for index, values in enumerate(before)]
    after_name = emit_values(plan["register_values_after_writes"], "after")
    lines.append("    static const AssignmentFieldWrite writes[] = {")
    for write, before_name in zip(writes, before_names, strict=True):
        location, value = write["address"], write["value"]
        is_global = location["kind"] == "global"
        is_constant = value["kind"] == "constant"
        width = {8: "Byte", 16: "Halfword", 32: "Word"}.get(write["width"])
        if width is None or location["kind"] not in ("global", "input_field") or \
                value["kind"] not in ("constant", "input_register"):
            raise ValueError(f"unsupported assignment at {address}")
        fields = (
            "AssignmentAddressKind::Global" if is_global else "AssignmentAddressKind::ObjectField",
            register("r3" if is_global else location["base_register"], "global_assignments"),
            f"0x{location['address_u32']:08x}u" if is_global else "0u",
            "0" if is_global else str(location["displacement"]),
            "AssignmentWidth::" + width,
            "AssignmentValueKind::Constant" if is_constant else "AssignmentValueKind::InputRegister",
            register("r3" if is_constant else value["register"], "global_assignments"),
            f"0x{value['value']:08x}u" if is_constant else "0u",
            before_name,
        )
        lines.append("        {" + ", ".join(fields) + "},")
    lines.append("    };")
    return lines, f"lo::semantic::gpu::WriteFieldAssignmentsWith(memory, registers, writes, {after_name});"


def copy_steps(entry: dict) -> list[tuple[str, str, str, int, int, int]]:
    steps = []
    for statement in entry["generated_effect_statements"]:
        if match := COPY_OFFSET.fullmatch(statement):
            destination, source, offset = match.groups()
            steps.append(("AddressBase", destination, source, int(offset), 0, 0))
        elif match := COPY_READ.fullmatch(statement):
            destination, bits, source, offset = match.groups()
            steps.append(("Read", destination, source, int(offset), int(bits), 0))
        elif match := COPY_WRITE.fullmatch(statement):
            bits, destination, offset, source, source_bits = match.groups()
            if bits != source_bits:
                raise ValueError(f"copy store width changed at {entry['address']}")
            steps.append(("Write", destination, source, int(offset), int(bits), 0))
        elif match := COPY_RETURN.fullmatch(statement):
            steps.append(("ReturnConstant", "r3", "r3", 0, 0, int(match[1])))
        else:
            raise ValueError(f"unreviewed copy step at {entry['address']}: {statement}")
    return steps


def field_operation(entry: dict) -> tuple[list[str], str]:
    address, family, params = entry["address"], entry["family"], entry["parameters"]
    lines: list[str] = []
    prefix = "lo::semantic::gpu::"
    if family.startswith("CopyFixedFields"):
        steps = copy_steps(entry)
        if not steps:
            raise ValueError(f"empty field copy {address}")
        lines.append("    static const FieldCopyStep steps[] = {")
        for kind, destination, source, offset, width, constant in steps:
            lines.append(f"        {{FieldCopyKind::{kind}, {register(destination, 'field_operations')}, "
                         f"{register(source, 'field_operations')}, {offset}, {width}, "
                         f"0x{constant & ((1 << 64) - 1):016x}ull}},")
        lines.append("    };")
        return lines, f"{prefix}{family}With(memory, registers, steps);"
    if family.startswith("ReadGlobal"):
        statements = entry["generated_effect_statements"]
        match = re.fullmatch(r"ctx\.r11\.s64 = (-?\d+);", statements[0])
        if match is None:
            raise ValueError(f"missing global lis at {address}")
        initial = int(match[1])
        cursor = 1
        adjustment = None
        if cursor < len(statements) and (match := COPY_OFFSET.fullmatch(statements[cursor])):
            if (match[1], match[2]) != ("r11", "r11"):
                raise ValueError(f"unsupported global adjustment at {address}")
            adjustment = int(match[3])
            cursor += 1
        if (initial + (adjustment or 0)) & 0xffffffff != params["global_base_u32"]:
            raise ValueError(f"global base mismatch at {address}")
        loads = params["ordered_loads"]
        if len(statements) - cursor != len(loads) or not loads:
            raise ValueError(f"global load count mismatch at {address}")
        lines.append("    static const GlobalFieldLoad loads[] = {")
        for index, item in enumerate(loads, cursor):
            match = COPY_READ.fullmatch(statements[index])
            if match is None or (match[1], int(match[2]), match[3], int(match[4])) != (
                    item["destination_register"], item["width"], item["base_register"], item["displacement"]):
                raise ValueError(f"global load changed at {address}")
            lines.append(f"        {{{register(item['destination_register'], 'field_operations')}, "
                         f"{item['displacement']}, {item['width']}}},")
        lines.append("    };")
        if family == "ReadGlobalField":
            optional = "std::nullopt" if adjustment is None else str(adjustment)
            call = f"{prefix}ReadGlobalFieldWith(memory, registers, 0x{initial & 0xffffffff:08x}u, {optional}, loads);"
        elif family == "ReadGlobalPointerChainField" and adjustment is None:
            call = f"{prefix}ReadGlobalPointerChainFieldWith(memory, registers, 0x{initial & 0xffffffff:08x}u, loads);"
        else:
            raise ValueError(f"unsupported global operation at {address}")
        return lines, call
    if family in ("FieldEqualsConstant", "FieldNotEqualsConstant"):
        call = (f"{prefix}{family}With(memory, registers, "
                f"{register(params['base_register'], 'field_operations')}, "
                f"{params['displacement']}, {params['width']}, {params['comparison_u32']}u);")
        return lines, call
    raise ValueError(f"unreviewed field operation {address}")


EMITTER = {
    "pointer_fields": pointer_operation,
    "global_assignments": global_operation,
    "field_operations": field_operation,
}


def generate(manifest: dict, family: str) -> str:
    """Return one translation unit containing all wrappers for a reviewed family."""
    entries = checked_entries(manifest, family)
    lines = [
        f"// Generated from {FAMILY_KIND[family]}.json; do not edit.",
        '#include "cpu/semantic_objects.h"',
        "using namespace lo::semantic::gpu;",
        "",
    ]
    for entry in entries:
        address = entry["address"]
        symbol = f"sub_{address}"
        setup, call = EMITTER[family](entry)
        lines.extend([
            f'extern "C" PPC_FUNC(__imp__{symbol});',
            f"PPC_FUNC({symbol})", "{",
            "    if (!lo::runtime::semantic_objects::Enabled())", "    {",
            f"        __imp__{symbol}(ctx, base);", "        return;", "    }",
            "    lo::runtime::semantic_objects::NativeObjectMemory memory(base);",
            *capture_registers(family), *setup, f"    {call}",
            *restore_registers(family), "}", "",
        ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pointer-manifest", type=Path, required=True)
    parser.add_argument("--global-manifest", type=Path, required=True)
    parser.add_argument("--field-manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sources = [generate(json.loads(path.read_text(encoding="utf-8")), family)
               for family, path in (
                   ("pointer_fields", args.pointer_manifest),
                   ("global_assignments", args.global_manifest),
                   ("field_operations", args.field_manifest),
               )]
    source = "\n".join(sources)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.is_file() or args.output.read_text(encoding="utf-8") != source:
        args.output.write_text(source, encoding="utf-8")
    print(f"Generated {source.count('PPC_FUNC(sub_')} semantic object wrappers")


if __name__ == "__main__":
    main()
