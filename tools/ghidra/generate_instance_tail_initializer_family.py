"""Check five exact tail wrappers and four bounded integer callees."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
CACHE = ROOT / "out/function-inventory"
MANIFEST = SEMANTICS / "instance_tail_initializer_families.json"
SOURCE = SEMANTICS / "src/instance_tail_initializer_family.cpp"
PARENTS = {
    "824192A0": "82419178",
    "8255DBC0": "8255DBD0",
    "825FC400": "825FC498",
    "826F89C8": "826B2850",
    "827CE228": "825A7F18",
}
NEW_CALLEES = {"82419178", "8255DBD0", "825FC498", "826B2850"}
STAGED = {"8255DBD0", "825FC498"}
CALL_INSTRUCTIONS = {
    "82419178": """mflr r12
stw r12,-8(r1)
std r30,-24(r1)
std r31,-16(r1)
stwu r1,-112(r1)
lis r11,-32231
mr r30,r3
addi r11,r11,7344
addi r3,r30,64
li r31,0
li r5,108
li r4,0
stw r11,0(r30)
stw r31,16(r3)
stw r31,20(r3)
stw r31,24(r3)
stw r31,72(r3)
stw r31,76(r3)
stw r31,80(r3)
stw r31,96(r3)
stw r31,100(r3)
stw r31,104(r3)
bl 0x82b7bc40
stw r31,172(r30)
mr r3,r30
stw r31,176(r30)
stw r31,180(r30)
stw r31,184(r30)
stw r31,188(r30)
stw r31,192(r30)
stw r31,196(r30)
stw r31,200(r30)
stw r31,204(r30)
stw r31,208(r30)
stw r31,212(r30)
stw r31,216(r30)
stw r31,220(r30)
stw r31,224(r30)
stw r31,228(r30)
addi r1,r1,112
lwz r12,-8(r1)
mtlr r12
ld r30,-24(r1)
ld r31,-16(r1)
blr""".splitlines(),
    "826B2850": """mflr r12
stw r12,-8(r1)
std r30,-24(r1)
std r31,-16(r1)
stwu r1,-112(r1)
lis r11,-32224
mr r30,r3
addi r10,r11,-3800
addi r31,r30,60
li r11,0
li r5,8
li r4,100
mr r3,r31
stw r10,0(r30)
stw r11,0(r31)
stw r11,4(r31)
stw r11,8(r31)
bl 0x8229f678
mr r3,r30
stw r30,12(r31)
addi r1,r1,112
lwz r12,-8(r1)
mtlr r12
ld r30,-24(r1)
ld r31,-16(r1)
blr""".splitlines(),
}
STAGED_BEGIN = "    // BEGIN GENERATED INSTANCE TAIL STAGED INITIALIZERS\n"
STAGED_END = "    // END GENERATED INSTANCE TAIL STAGED INITIALIZERS\n"
SOUND_BEGIN = "    // BEGIN GENERATED INSTANCE TAIL SOUND INITIALIZER\n"
SOUND_END = "    // END GENERATED INSTANCE TAIL SOUND INITIALIZER\n"
ENTRIES_BEGIN = "    // BEGIN GENERATED INSTANCE TAIL ENTRIES\n"
ENTRIES_END = "    // END GENERATED INSTANCE TAIL ENTRIES\n"


def signed(value: str) -> int:
    result = int(value)
    if not -32768 <= result <= 32767:
        raise ValueError(f"non-16-bit immediate: {value}")
    return result


def cpp_address(base: str, offset: int) -> str:
    if offset < 0:
        return f"{base} - {-offset}u"
    return f"{base} + {offset}u"


def staged_body(address: str, body: list[str]) -> tuple[list[str], list[str]]:
    if body[:2] != [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
                    "PPC_FUNC_PROLOGUE();"] or body[-2:] != ["return;", "}"]:
        raise ValueError(f"{address}: staged body boundary changed")
    instructions = []
    statements = []
    registers: dict[str, int | str] = {}
    generated = []
    stage = 0
    index = 2
    while index < len(body) - 1:
        comment = body[index]
        if not comment.startswith("// ") or index + 1 >= len(body) - 1:
            raise ValueError(f"{address}: untranslated staged statement")
        instruction = comment[3:]
        actual = body[index + 1]
        instructions.append(instruction)
        translated: str
        if instruction == "blr":
            translated = "return;"
        elif match := re.fullmatch(r"lis (r\d+),(-?\d+)", instruction):
            destination, immediate = match.groups()
            value = signed(immediate) << 16
            registers[destination] = value
            translated = f"ctx.{destination}.s64 = {value};"
        elif match := re.fullmatch(r"li (r\d+),(-?\d+)", instruction):
            destination, immediate = match.groups()
            value = signed(immediate)
            registers[destination] = value
            translated = f"ctx.{destination}.s64 = {value};"
        elif match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction):
            destination, source, immediate = match.groups()
            if not isinstance(registers.get(source), int):
                raise ValueError(f"{address}: addi source not constant")
            value = signed(immediate)
            registers[destination] = registers[source] + value
            translated = f"ctx.{destination}.s64 = ctx.{source}.s64 + {value};"
        elif match := re.fullmatch(r"rotlwi (r\d+),(r\d+),0", instruction):
            destination, source = match.groups()
            if source not in registers:
                raise ValueError(f"{address}: rotate source undefined")
            value = registers[source]
            registers[destination] = value & 0xffffffff if isinstance(value, int) else value
            translated = f"ctx.{destination}.u64 = __builtin_rotateleft32(ctx.{source}.u32, 0);"
        elif match := re.fullmatch(r"stw (r\d+),(-?\d+)\((r1|r3)\)", instruction):
            source, displacement, base = match.groups()
            if source not in registers:
                raise ValueError(f"{address}: store source undefined")
            offset = signed(displacement)
            translated = (f"PPC_STORE_U32(ctx.{base}.u32 + {offset}, "
                          f"ctx.{source}.u32);")
            value = registers[source]
            operand = (f"0x{value & 0xffffffff:08x}u" if isinstance(value, int)
                       else value)
            base_expression = "object" if base == "r3" else "caller_sp"
            generated.append(f"    memory.WriteU32({cpp_address(base_expression, offset)}, {operand});")
        elif match := re.fullmatch(r"lwz (r\d+),(-?\d+)\(r1\)", instruction):
            destination, displacement = match.groups()
            offset = signed(displacement)
            translated = f"ctx.{destination}.u64 = PPC_LOAD_U32(ctx.r1.u32 + {offset});"
            stage += 1
            variable = f"staged_{stage}"
            registers[destination] = variable
            generated.append(f"    const std::uint32_t {variable} = "
                             f"memory.ReadU32({cpp_address('caller_sp', offset)});")
        else:
            raise ValueError(f"{address}: unsupported staged opcode: {instruction}")
        if actual != translated:
            raise ValueError(f"{address}: translated PPC changed at {instruction}")
        statements.append(actual)
        index += 2
    if instructions[-1:] != ["blr"] or index != len(body) - 1:
        raise ValueError(f"{address}: missing blr or extra body")
    return instructions, generated


def parent_body(address: str, callee: str, body: list[str]) -> None:
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
                "PPC_FUNC_PROLOGUE();", "// cmplwi cr6,r3,0",
                "ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);",
                "// beqlr cr6", "if (ctx.cr6.eq) return;",
                f"// b 0x{callee.lower()}", f"sub_{callee}(ctx, base);",
                "return;", "}"]
    if body != expected:
        raise ValueError(f"{address}: full parent body/CFG changed")


def helper_call_body(address: str, body: list[str]) -> None:
    """Rebuild every emitted line of the two branch-free helper-call bodies."""
    instructions = CALL_INSTRUCTIONS[address]
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{",
                "PPC_FUNC_PROLOGUE();", "PPCRegister temp{};"]
    for index, instruction in enumerate(instructions):
        statements: list[str]
        if instruction == "mflr r12":
            statements = ["ctx.r12.u64 = ctx.lr;"]
        elif instruction == "mtlr r12":
            statements = ["ctx.lr = ctx.r12.u64;"]
        elif instruction == "blr":
            statements = ["return;"]
        elif match := re.fullmatch(r"stwu r1,(-?\d+)\(r1\)", instruction):
            offset = signed(match.group(1))
            statements = [f"temp.u64 = ctx.r1.u64 + uint64_t({offset});",
                          "PPC_STORE_U32(temp.u32, ctx.r1.u32);",
                          "ctx.r1.u64 = temp.u64;"]
        elif match := re.fullmatch(r"(stw|std) (r\d+),(-?\d+)\((r\d+)\)", instruction):
            operation, source, displacement, base = match.groups()
            offset = signed(displacement)
            width = "U32" if operation == "stw" else "U64"
            field = "u32" if operation == "stw" else "u64"
            statements = [f"PPC_STORE_{width}(ctx.{base}.u32 + {offset}, "
                          f"ctx.{source}.{field});"]
        elif match := re.fullmatch(r"(lwz|ld) (r\d+),(-?\d+)\((r\d+)\)", instruction):
            operation, destination, displacement, base = match.groups()
            offset = signed(displacement)
            width = "U32" if operation == "lwz" else "U64"
            statements = [f"ctx.{destination}.u64 = "
                          f"PPC_LOAD_{width}(ctx.{base}.u32 + {offset});"]
        elif match := re.fullmatch(r"lis (r\d+),(-?\d+)", instruction):
            destination, immediate = match.groups()
            statements = [f"ctx.{destination}.s64 = {signed(immediate) << 16};"]
        elif match := re.fullmatch(r"li (r\d+),(-?\d+)", instruction):
            destination, immediate = match.groups()
            statements = [f"ctx.{destination}.s64 = {signed(immediate)};"]
        elif match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction):
            destination, source, immediate = match.groups()
            statements = [f"ctx.{destination}.s64 = ctx.{source}.s64 + "
                          f"{signed(immediate)};"]
        elif match := re.fullmatch(r"mr (r\d+),(r\d+)", instruction):
            destination, source = match.groups()
            statements = [f"ctx.{destination}.u64 = ctx.{source}.u64;"]
        elif match := re.fullmatch(r"bl 0x([0-9a-f]{8})", instruction):
            target = match.group(1).upper()
            return_pc = int(address, 16) + (index + 1) * 4
            statements = [f"ctx.lr = 0x{return_pc:08X};",
                          f"sub_{target}(ctx, base);"]
        else:
            raise ValueError(f"{address}: unsupported helper-call opcode: {instruction}")
        expected.extend([f"// {instruction}", *statements])
    expected.append("}")
    if body != expected:
        raise ValueError(f"{address}: full translated helper-call body/CFG changed")


def reused_pointer_body(callee: dict, source: str) -> None:
    address = "825A7F18"
    pointer = json.loads((SEMANTICS / "pointer_fields_families.json")
                         .read_text(encoding="utf-8"))
    matches = [entry for entry in pointer["entries"] if entry["address"] == address]
    if len(matches) != 1:
        raise ValueError("reused pointer callee missing or duplicated")
    entry = matches[0]
    instructions = ["lis r11,-32226", "addi r11,r11,-24416",
                    "stw r11,0(r3)", "blr"]
    value = ((-32226 << 16) - 24416) & 0xffffffff
    parameters = entry["parameters"]
    if entry["family"] != "InitializeConstantFields" or \
            entry["instruction_sequence"] != instructions or \
            parameters["base_register"] != "r3" or \
            parameters["ordered_writes"] != [{
                "instruction_index": 2, "base_register": "r3",
                "displacement": 0, "width": 32, "value": value,
                "source_register": "r11"}]:
        raise ValueError("reused pointer parameters changed")
    effects = entry["instruction_effects"]
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{", "PPC_FUNC_PROLOGUE();"]
    for instruction, effect in zip(instructions, effects, strict=True):
        if effect["instruction"] != instruction:
            raise ValueError("reused pointer instruction effects changed")
        expected.extend([f"// {instruction}", *effect["statements"]])
    expected.append("}")
    if callee["body"] != expected or callee["instructions"] != instructions:
        raise ValueError("reused pointer full translated body changed")
    constant = f"0x{value:08x}u"
    if source.count(f"PointerFieldRegister::R11, {constant}") != 1 or \
            source.count(f"PointerFieldWidth::Word, {constant}") != 1:
        raise ValueError("reused pointer call parameters changed")


def source_region(original: str, begin: str, end: str, content: str) -> str:
    if original.count(begin) != 1 or original.count(end) != 1:
        raise ValueError("source marker changed")
    left, rest = original.split(begin)
    _, right = rest.split(end)
    return left + begin + content + end + right


def generate() -> tuple[dict, str]:
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    candidate_map = {entry["address"]: entry for entry in candidates["entries"]}
    originals = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    cached = json.loads((CACHE / "instance-tail-callees.json")
                        .read_text(encoding="utf-8"))
    callee_map = {entry["address"]: entry for entry in cached["callees"]}
    if len(cached["parents"]) != 23 or len(cached["callees"]) != 23 or \
            set(PARENTS.values()) - set(callee_map):
        raise ValueError("tail callee inventory incomplete")
    if {entry["address"] for entry in cached["parents"] if
            entry["address"] in PARENTS} != set(PARENTS):
        raise ValueError("tail parent inventory changed")

    entries = []
    for parent, callee in sorted(PARENTS.items()):
        candidate = candidate_map[parent]
        if candidate["instructions"] != ["cmplwi cr6,r3,0", "beqlr cr6",
                                           f"b 0x{callee.lower()}"] or \
                candidate["cfg_branches"] or candidate["direct_calls"]:
            raise ValueError(f"{parent}: parent instructions/CFG changed")
        body = originals[parent]
        parent_body(parent, callee, body)
        entries.append({"address": parent, "kind": "null_guarded_tail",
                        "callee": callee, "source": candidate["generated_ppc_path"],
                        "source_line": candidate["line"],
                        "instruction_sequence": candidate["instructions"],
                        "cfg_branches": [], "direct_calls": [], "body": body})
    source = SOURCE.read_text(encoding="utf-8")
    reused_pointer_body(callee_map["825A7F18"], source)
    rows = {callee: callee for callee in NEW_CALLEES}
    rows.update(PARENTS)
    table = "".join(f"    {{0x{address.lower()}u, 0x{callee.lower()}u, "
                    f"{'true' if address in PARENTS else 'false'}}},\n"
                    for address, callee in sorted(rows.items()))
    source = source_region(source, ENTRIES_BEGIN, ENTRIES_END, table)
    for callee in sorted(NEW_CALLEES):
        entry = callee_map[callee]
        body = entry["body"]
        instructions = entry["instructions"]
        if [line[3:] for line in body if line.startswith("// ")] != instructions:
            raise ValueError(f"{callee}: cached body comments changed")
        if callee in STAGED:
            parsed, code = staged_body(callee, body)
            if parsed != instructions:
                raise ValueError(f"{callee}: staged body not exhaustive")
            begin, end = ((STAGED_BEGIN, STAGED_END) if callee == "8255DBD0"
                          else (SOUND_BEGIN, SOUND_END))
            source = source_region(source, begin, end, "\n".join(code) + "\n")
        else:
            if instructions != CALL_INSTRUCTIONS[callee]:
                raise ValueError(f"{callee}: closed helper-call instructions changed")
            helper_call_body(callee, body)
        direct_calls = [part.split("0x", 1)[1].upper() for part in instructions
                        if part.startswith("bl 0x")]
        if callee in STAGED and direct_calls:
            raise ValueError(f"{callee}: staged initializer gained a call")
        entries.append({"address": callee, "kind": "closed_integer_callee",
                        "source": entry["source"], "source_line": entry["source_line"],
                        "instruction_sequence": instructions,
                        "cfg_branches": [], "direct_calls": direct_calls,
                        "body": body})
    manifest = {"schema_version": 1, "entry_count": len(entries),
                "parent_count": len(PARENTS), "new_callee_count": len(NEW_CALLEES),
                "reused_callee": "825A7F18", "entries": sorted(entries,
                key=lambda item: item["address"]),
                "bounded_effects": "object and staged stack words, full r3; generic volatile registers and helper ABI spills excluded"}
    return manifest, source


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    manifest, source = generate()
    encoded = json.dumps(manifest, indent=2) + "\n"
    if args.write:
        write_if_changed(MANIFEST, encoded)
        write_if_changed(SOURCE, source)
    else:
        if MANIFEST.read_text(encoding="utf-8") != encoded or \
                SOURCE.read_text(encoding="utf-8") != source:
            raise ValueError("tail family manifest/source differs from exact cached PPC")
    print(f"PASS {len(PARENTS)} tail parents, {len(NEW_CALLEES)} new callees, 1 reused callee")


if __name__ == "__main__":
    main()
