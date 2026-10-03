"""Validate the exact RPNavi wrapper/callee bodies and emit shared parameters."""

from __future__ import annotations

import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed


ROOT = Path(__file__).resolve().parents[2]
PARENTS = {
    "82B55028": "82B54518", "82B55068": "82B54690",
    "82B550A8": "82B548F8", "82B550E8": "82B54980",
    "82B55128": "82B54AF8", "82B55168": "82B54C70",
    "82B551A8": "82B54DE8", "82B56728": "82B55FF8",
}
BEGIN = "    // BEGIN GENERATED INSTANCE NAVIGATION PARAMETERS\n"
END = "    // END GENERATED INSTANCE NAVIGATION PARAMETERS\n"


def effect(instruction: str, pc: int) -> list[str]:
    if instruction == "mflr r12":
        return ["ctx.r12.u64 = ctx.lr;"]
    if instruction == "mtlr r12":
        return ["ctx.lr = ctx.r12.u64;"]
    if instruction == "mr r13,r13":
        return ["ctx.r13.u64 = ctx.r13.u64;"]
    if instruction == "mr r14,r14":
        return ["ctx.r14.u64 = ctx.r14.u64;"]
    if instruction == "blr":
        return ["return;"]
    match = re.fullmatch(r"mr (r\d+),(r\d+)", instruction)
    if match:
        dst, src = match.groups()
        return [f"ctx.{dst}.u64 = ctx.{src}.u64;"]
    match = re.fullmatch(r"lis (r\d+),(-?\d+)", instruction)
    if match:
        dst, hi = match.groups()
        return [f"ctx.{dst}.s64 = {int(hi) << 16};"]
    match = re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction)
    if match:
        dst, src, imm = match.groups()
        return [f"ctx.{dst}.s64 = ctx.{src}.s64 + {int(imm)};"]
    match = re.fullmatch(r"stw (r\d+),(-?\d+)\((r\d+)\)", instruction)
    if match:
        src, disp, base = match.groups()
        return [f"PPC_STORE_U32(ctx.{base}.u32 + {int(disp)}, ctx.{src}.u32);"]
    match = re.fullmatch(r"std (r\d+),(-?\d+)\((r\d+)\)", instruction)
    if match:
        src, disp, base = match.groups()
        return [f"PPC_STORE_U64(ctx.{base}.u32 + {int(disp)}, ctx.{src}.u64);"]
    match = re.fullmatch(r"lwz (r\d+),(-?\d+)\((r\d+)\)", instruction)
    if match:
        dst, disp, base = match.groups()
        return [f"ctx.{dst}.u64 = PPC_LOAD_U32(ctx.{base}.u32 + {int(disp)});"]
    match = re.fullmatch(r"ld (r\d+),(-?\d+)\((r\d+)\)", instruction)
    if match:
        dst, disp, base = match.groups()
        return [f"ctx.{dst}.u64 = PPC_LOAD_U64(ctx.{base}.u32 + {int(disp)});"]
    match = re.fullmatch(r"stwu r1,-(96|112)\(r1\)", instruction)
    if match:
        size = int(match.group(1))
        return [f"temp.u64 = ctx.r1.u64 + uint64_t(-{size});",
                "PPC_STORE_U32(temp.u32, ctx.r1.u32);", "ctx.r1.u64 = temp.u64;"]
    if instruction == "cmplwi cr6,r31,0":
        return ["ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);"]
    match = re.fullmatch(r"beq cr6,0x([0-9a-f]+)", instruction)
    if match:
        return [f"if (ctx.cr6.eq) goto loc_{match.group(1).upper()};"]
    match = re.fullmatch(r"bl 0x([0-9a-f]+)", instruction)
    if match:
        return [f"ctx.lr = 0x{pc + 4:08X};",
                f"sub_{match.group(1).upper()}(ctx, base);"]
    raise ValueError(f"unmapped RPNavi instruction: {instruction}")


def validate_body(address: str, instructions: list[str], body: list[str]) -> None:
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
                "PPC_FUNC_PROLOGUE();", "PPCRegister temp{};"]
    for index, instruction in enumerate(instructions):
        if index and instructions[index - 1].startswith("bl ") and \
                address in PARENTS:
            expected.append(f"loc_{int(address, 16) + 40:08X}:")
        expected.append(f"// {instruction}")
        expected.extend(effect(instruction, int(address, 16) + index * 4))
    expected.append("}")
    if body != expected:
        for index, (actual, wanted) in enumerate(zip(body, expected)):
            if actual != wanted:
                raise ValueError(f"full body changed at {address} line {index}: {actual!r} != {wanted!r}")
        raise ValueError(f"full body length changed: {address} {len(body)} != {len(expected)}")


def validate_parent(entry: dict, body: list[str]) -> str:
    address = entry["address"]
    callee = PARENTS[address]
    branch = f"0x{int(address, 16) + 40:08x}"
    expected = ["mflr r12", "stw r12,-8(r1)", "std r31,-16(r1)",
                "stwu r1,-96(r1)", "mr r31,r3", "mr r13,r13",
                "cmplwi cr6,r31,0", f"beq cr6,{branch}",
                "mr r3,r31", f"bl 0x{callee.lower()}", "mr r14,r14",
                "addi r1,r1,96", "lwz r12,-8(r1)", "mtlr r12",
                "ld r31,-16(r1)", "blr"]
    if entry["instructions"] != expected or entry["cfg_branches"] != [[7, "beq", 10]] or \
            entry["direct_calls"] != [callee]:
        raise ValueError(f"parent CFG or instructions changed: {address}")
    validate_body(address, expected, body)
    return entry["types"][0]["type_name"]


def validate_callee(entry: dict) -> tuple[str, str, bool]:
    address = entry["address"]
    ins = entry["instructions"]
    prefix = ["mflr r12", "stw r12,-8(r1)", "std r30,-24(r1)",
              "std r31,-16(r1)", "addi r31,r1,-112", "stwu r1,-112(r1)",
              "mr r30,r3", "stw r30,132(r31)", "mr r13,r13", "mr r3,r30"]
    suffix = ["mr r14,r14", "mr r3,r30", "addi r1,r31,112",
              "lwz r12,-8(r1)", "mtlr r12", "ld r30,-24(r1)",
              "ld r31,-16(r1)", "blr"]
    volume = address == "82B55FF8"
    middle = (["addi r10,r30,576", "addi r11,r11,4096",
               "stw r10,80(r31)", "stw r11,0(r30)"] +
              [part for offset in (588, 616, 628, 640, 652, 664, 676)
               for part in (f"addi r11,r30,{offset}", "stw r11,80(r31)")]
              if volume else ["addi r11,r11," +
                   re.fullmatch(r"addi r11,r11,(-?\d+)", ins[12]).group(1),
                   "stw r11,0(r30)"])
    match = re.fullmatch(r"bl 0x([0-9a-f]+)", ins[10])
    hi = re.fullmatch(r"lis r11,(-?\d+)", ins[11])
    if not match or not hi or ins != prefix + [ins[10], ins[11]] + middle + suffix:
        raise ValueError(f"callee CFG or instructions changed: {address}")
    lower = match.group(1).upper()
    if entry["direct_calls"] != [lower] or len(ins) != (38 if volume else 22):
        raise ValueError(f"callee call edge changed: {address}")
    body = [line.strip() for line in entry["body"].splitlines()]
    validate_body(address, ins, body)
    low = 4096 if volume else int(re.fullmatch(r"addi r11,r11,(-?\d+)", ins[12]).group(1))
    return lower, signed_word(int(hi.group(1)), low, address), volume


def main() -> None:
    candidates = json.loads((ROOT / "out/function-inventory/registered-instance-candidates.json").read_text())
    cached = json.loads((ROOT / "out/function-inventory/instance-navigation-callees.json").read_text())
    pointer = json.loads((ROOT / "LostOdysseyRecompSemantics/pointer_fields_families.json").read_text())
    bodies = raw_bodies(ROOT / "out/function-inventory/registered-instance-originals.cpp.gz")
    if len(candidates["entries"]) != 786 or cached["entry_count"] != 8:
        raise ValueError("cached instance inventory changed")
    parents = {entry["address"]: entry for entry in candidates["entries"]}
    callees = {entry["address"]: entry for entry in cached["entries"]}
    lowers = {entry["address"]: entry for entry in pointer["entries"]}
    if set(callees) != set(PARENTS.values()):
        raise ValueError("RPNavi callee set changed")
    specs = []
    for address, callee in sorted(PARENTS.items()):
        name = validate_parent(parents[address], bodies[address])
        lower, derived, volume = validate_callee(callees[callee])
        if lower == "82B548F8":
            if callee != "82B54980":
                raise ValueError("unexpected nested RPNavi callee")
            base = "0x00000000"
        else:
            dependency = lowers.get(lower)
            if dependency is None or dependency["family"] != "InitializeConstantFields" or \
                    len(dependency["instruction_sequence"]) != 4:
                raise ValueError(f"RPNavi lower is not a recovered constant-field initializer: {lower}")
            writes = dependency["parameters"]["ordered_writes"]
            if len(writes) != 1 or writes[0]["displacement"] != 0 or \
                    writes[0]["width"] != 32 or writes[0]["base_register"] != "r3":
                raise ValueError(f"RPNavi lower field layout changed: {lower}")
            base = f"0x{writes[0]['value']:08x}"
        specs.append({"address": address, "callee": callee, "lower": lower,
                      "base_vtable": base, "derived_vtable": derived,
                      "volume_scratch": volume, "type_name": name,
                      "source": parents[address]["generated_ppc_path"],
                      "source_line": parents[address]["line"],
                      "callee_source": callees[callee]["generated_ppc_path"],
                      "callee_source_line": callees[callee]["line"]})
    entries = []
    for spec in specs:
        entries.append({"address": spec["address"], "kind": "null_guarded_wrapper",
                        "callee": spec["callee"], "source": spec["source"],
                        "source_line": spec["source_line"]})
        entries.append({"address": spec["callee"], "kind": "derived_initializer",
                        "lower": spec["lower"], "source": spec["callee_source"],
                        "source_line": spec["callee_source_line"]})
    entries.sort(key=lambda entry: entry["address"])
    if len({entry["address"] for entry in entries}) != 16:
        raise ValueError("navigation entry identities overlap")
    manifest = {"schema_version": 1, "family": "RPNaviConstructorChain",
                "entry_count": 16, "wrapper_count": 8, "callee_count": 8,
                "entries": entries, "chains": specs,
                "limitations": ["ordinary mapped-memory observations only; 64-bit std is represented as two word writes",
                                "temporary GPR/CR/LR and fault/MMIO observations are outside the API"]}
    manifest_path = ROOT / "LostOdysseyRecompSemantics/instance_navigation_initializer_families.json"
    write_if_changed(manifest_path, json.dumps(manifest, indent=2) + "\n")
    source_path = ROOT / "LostOdysseyRecompSemantics/src/instance_navigation_initializer_family.cpp"
    source = source_path.read_text(encoding="utf-8")
    table = "".join(f"    {{0x{item['address'].lower()}u, 0x{item['callee'].lower()}u, "
                    f"0x{item['lower'].lower()}u, {item['base_vtable'].lower()}u, "
                    f"{item['derived_vtable'].lower()}u, "
                    f"{'true' if item['volume_scratch'] else 'false'}}},\n"
                    for item in specs)
    start = source.index(BEGIN) + len(BEGIN)
    end = source.index(END, start)
    write_if_changed(source_path, source[:start] + table + source[end:])
    print("validated 8 exact wrappers, 8 exact callees, 5 recovered lower dependencies")


if __name__ == "__main__":
    main()
