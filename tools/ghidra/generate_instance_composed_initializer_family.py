"""Validate eleven complete PPC bodies and emit closed instance compositions."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_vtable_family import raw_bodies, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "out/function-inventory"
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "instance_composed_initializer_families.json"
SOURCE = SEMANTICS / "src/instance_composed_initializer_family.cpp"
BEGIN = "    // BEGIN GENERATED INSTANCE COMPOSED PARAMETERS\n"
END = "    // END GENERATED INSTANCE COMPOSED PARAMETERS\n"
CALLEES = {"825A56D0", "826DB6A8", "826B50C8", "826D6C30"}
TAILS = {"825A80B0": "825A56D0", "825E0928": "826DB6A8",
         "826B50B8": "826B50C8", "826D6C20": "826D6C30"}
WRAPPERS = {"8262E498": ("825A56D0", 0x821E8910, 0),
            "82716058": ("826DB6A8", 0x8220CBB8, 0),
            "8272CE60": ("826B50C8", 0x82210B68, 0x82210C78)}
COUNTS = {"825A56D0": 36, "826DB6A8": 36, "826B50C8": 50,
          "826D6C30": 57, "8262E498": 16, "82716058": 13,
          "8272CE60": 19}
BRANCHES = {"825A56D0": ["bne cr6,0x825a573c", "beq cr6,0x825a5734",
                            "b 0x825a5738"],
            "826DB6A8": [], "826B50C8": ["bne cr6,0x826b5120"],
            "826D6C30": ["bne cr6,0x826d6ce0"],
            "8262E498": ["beq cr6,0x8262e4c4"],
            "82716058": ["beq cr6,0x8271607c"],
            "8272CE60": ["beq cr6,0x8272ce98"]}
DIRECT_CALL = {"825A56D0": "82486C88", "826B50C8": "825F41E8",
               "826D6C30": "825F41E8", "8262E498": "825A56D0",
               "82716058": "826DB6A8", "8272CE60": "826B50C8"}


def immediate(text: str) -> int:
    value = int(text)
    if not -32768 <= value <= 32767:
        raise ValueError(f"non-16-bit immediate: {text}")
    return value


def expected_body(address: str, instructions: list[str]) -> list[str]:
    """Reconstruct every translated statement, label and call-site LR."""
    target_pcs = {int(address, 16) + 4 * index
                  for index in range(len(instructions))}
    labels = set()
    for instruction in instructions:
        match = re.fullmatch(r"(?:beq cr6,|bne cr6,|b )0x([0-9a-f]{8})", instruction)
        if match and int(match.group(1), 16) in target_pcs:
            labels.add(int(match.group(1), 16))
    needs_temp = any(part.startswith(("stwu ", "lfs ", "stfs "))
                     for part in instructions)
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{", "PPC_FUNC_PROLOGUE();"]
    if needs_temp:
        expected.append("PPCRegister temp{};")
    first_lfs = True
    for index, instruction in enumerate(instructions):
        pc = int(address, 16) + index * 4
        if pc in labels:
            expected.append(f"loc_{pc:08X}:")
        statements: list[str]
        if instruction == "mflr r12":
            statements = ["ctx.r12.u64 = ctx.lr;"]
        elif instruction == "mtlr r12":
            statements = ["ctx.lr = ctx.r12.u64;"]
        elif instruction == "blr":
            statements = ["return;"]
        elif instruction == "beqlr cr6":
            statements = ["if (ctx.cr6.eq) return;"]
        elif match := re.fullmatch(r"stwu r1,(-?\d+)\(r1\)", instruction):
            offset = immediate(match.group(1))
            statements = [f"temp.u64 = ctx.r1.u64 + uint64_t({offset});",
                          "PPC_STORE_U32(temp.u32, ctx.r1.u32);",
                          "ctx.r1.u64 = temp.u64;"]
        elif match := re.fullmatch(r"(stw|std) (r\d+),(-?\d+)\((r\d+)\)", instruction):
            operation, source, displacement, base = match.groups()
            offset = immediate(displacement)
            width, field = ("U32", "u32") if operation == "stw" else ("U64", "u64")
            statements = [f"PPC_STORE_{width}(ctx.{base}.u32 + {offset}, "
                          f"ctx.{source}.{field});"]
        elif match := re.fullmatch(r"(lwz|ld) (r\d+),(-?\d+)\((r\d+)\)", instruction):
            operation, destination, displacement, base = match.groups()
            width = "U32" if operation == "lwz" else "U64"
            statements = [f"ctx.{destination}.u64 = PPC_LOAD_{width}"
                          f"(ctx.{base}.u32 + {immediate(displacement)});"]
        elif match := re.fullmatch(r"lis (r\d+),(-?\d+)", instruction):
            register, value = match.groups()
            statements = [f"ctx.{register}.s64 = {immediate(value) << 16};"]
        elif match := re.fullmatch(r"li (r\d+),(-?\d+)", instruction):
            register, value = match.groups()
            statements = [f"ctx.{register}.s64 = {immediate(value)};"]
        elif match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction):
            destination, source, value = match.groups()
            statements = [f"ctx.{destination}.s64 = ctx.{source}.s64 + "
                          f"{immediate(value)};"]
        elif match := re.fullmatch(r"mr (r\d+),(r\d+)", instruction):
            destination, source = match.groups()
            statements = [f"ctx.{destination}.u64 = ctx.{source}.u64;"]
        elif match := re.fullmatch(r"(cmplwi|cmpldi|cmpwi) cr6,(r\d+),0", instruction):
            operation, source = match.groups()
            kind, field = {"cmplwi": ("uint32_t", "u32"),
                           "cmpldi": ("uint64_t", "u64"),
                           "cmpwi": ("int32_t", "s32")}[operation]
            statements = [f"ctx.cr6.compare<{kind}>(ctx.{source}.{field}, 0, ctx.xer);"]
        elif match := re.fullmatch(r"(beq|bne) cr6,0x([0-9a-f]{8})", instruction):
            operation, target = match.groups()
            if int(target, 16) not in target_pcs:
                raise ValueError(f"{address}: conditional branch leaves function")
            condition = "ctx.cr6.eq" if operation == "beq" else "!ctx.cr6.eq"
            statements = [f"if ({condition}) goto loc_{target.upper()};"]
        elif match := re.fullmatch(r"b 0x([0-9a-f]{8})", instruction):
            target = match.group(1)
            if int(target, 16) in target_pcs:
                statements = [f"goto loc_{target.upper()};"]
            else:
                statements = [f"sub_{target.upper()}(ctx, base);", "return;"]
        elif match := re.fullmatch(r"bl 0x([0-9a-f]{8})", instruction):
            target = match.group(1).upper()
            statements = [f"ctx.lr = 0x{pc + 4:08X};",
                          f"sub_{target}(ctx, base);"]
        elif instruction == "rlwinm r10,r10,0,22,22":
            statements = ["ctx.r10.u64 = __builtin_rotateleft64"
                          "(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x200;"]
        elif match := re.fullmatch(r"lfs (f\d+),(-?\d+)\((r\d+)\)", instruction):
            destination, displacement, base = match.groups()
            statements = []
            if first_lfs:
                statements.append("ctx.fpscr.disableFlushMode();")
                first_lfs = False
            statements.extend([f"temp.u32 = PPC_LOAD_U32(ctx.{base}.u32 + "
                               f"{immediate(displacement)});",
                               f"ctx.{destination}.f64 = double(temp.f32);"])
        elif match := re.fullmatch(r"stfs (f\d+),(-?\d+)\((r\d+)\)", instruction):
            source, displacement, base = match.groups()
            statements = [f"temp.f32 = float(ctx.{source}.f64);",
                          f"PPC_STORE_U32(ctx.{base}.u32 + "
                          f"{immediate(displacement)}, temp.u32);"]
        else:
            raise ValueError(f"{address}: unsupported instruction: {instruction}")
        expected.extend([f"// {instruction}", *statements])
    expected.append("}")
    return expected


def parameters(address: str, instructions: list[str]) -> tuple[str, int, int, bool]:
    if address in CALLEES:
        callee, guard, vtable, inner = address, False, 0, 0
    elif address in TAILS:
        callee, guard, vtable, inner = TAILS[address], True, 0, 0
        if instructions != ["cmplwi cr6,r3,0", "beqlr cr6",
                            f"b 0x{callee.lower()}"]:
            raise ValueError(f"{address}: tail guard/callee changed")
    else:
        callee, vtable, inner = WRAPPERS[address]
        guard = False
        if instructions.count(f"bl 0x{callee.lower()}") != 1 or \
                not any(part.startswith("beq cr6,") for part in instructions):
            raise ValueError(f"{address}: wrapper control flow changed")
        constants: dict[str, int] = {}
        stores = []
        for instruction in instructions:
            if match := re.fullmatch(r"lis (r\d+),(-?\d+)", instruction):
                register, literal = match.groups()
                constants[register] = immediate(literal) << 16
            elif match := re.fullmatch(r"li (r\d+),(-?\d+)", instruction):
                register, literal = match.groups()
                constants[register] = immediate(literal)
            elif match := re.fullmatch(r"addi (r\d+),(r\d+),(-?\d+)", instruction):
                register, source, literal = match.groups()
                if source in constants:
                    constants[register] = constants[source] + immediate(literal)
            elif instruction.startswith("bl 0x"):
                constants = {register: value for register, value in constants.items()
                             if int(register[1:]) >= 14}
            elif match := re.fullmatch(r"stw (r\d+),(\d+)\((r\d+)\)", instruction):
                register, offset, base = match.groups()
                if register in constants:
                    stores.append((base, int(offset), constants[register] & 0xffffffff))
        base = "r3" if address == "82716058" else "r31"
        expected = [(base, 0, vtable)]
        if inner:
            expected.append(("r31", 60, inner))
        if stores != expected:
            raise ValueError(f"{address}: wrapper vtable constants/order changed: {stores}")
    return callee, vtable, inner, guard


def lower_evidence() -> list[dict]:
    recovery = json.loads((SEMANTICS / "recovery.json").read_text(encoding="utf-8"))
    expected = {"82486C88": ("lo::semantic::gpu::AllocateManagerBuffer",
                             "tools/ghidra/test_semantic_manager_facade.py"),
                "825F41E8": ("lo::semantic::gpu::registered_metadata_words::AppendMetadataWord",
                             "tools/ghidra/test_semantic_registered_metadata_words.py")}
    proof = []
    for address, (name, runner) in expected.items():
        matches = [entry for entry in recovery["functions"] if
                   entry["address"] == address]
        if len(matches) != 1 or matches[0]["name"] != name or \
                not all(matches[0]["stages"][stage] for stage in
                        ("readable_implementation", "differential_validation")) or \
                not any(item["path"] == runner for item in matches[0]["evidence"]):
            raise ValueError(f"{address}: recovered lower helper proof changed")
        proof.append({"address": address, "implementation": name,
                      "bounded_oracle": runner})
    return proof


def generate() -> tuple[dict, str]:
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    candidate_map = {entry["address"]: entry for entry in candidates["entries"]}
    originals = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    callee_cache = json.loads((CACHE / "instance-tail-callees.json")
                              .read_text(encoding="utf-8"))
    callee_map = {entry["address"]: entry for entry in callee_cache["callees"]}
    if len(callee_cache["callees"]) != 23 or len(callee_cache["parents"]) != 23:
        raise ValueError("callee cache incomplete")
    addresses = CALLEES | set(TAILS) | set(WRAPPERS)
    if len(addresses) != 11 or not CALLEES <= set(callee_map) or \
            not (set(TAILS) | set(WRAPPERS)) <= set(candidate_map):
        raise ValueError("composed target set changed")
    entries = []
    rows = []
    for address in sorted(addresses):
        raw = callee_map[address] if address in CALLEES else candidate_map[address]
        instructions = raw["instructions"]
        body = raw["body"] if address in CALLEES else originals[address]
        if address in COUNTS and len(instructions) != COUNTS[address] or \
                address in TAILS and len(instructions) != 3:
            raise ValueError(f"{address}: instruction count changed")
        if body != expected_body(address, instructions):
            raise ValueError(f"{address}: complete translated body/CFG changed")
        actual_branches = [part for part in instructions if
                           part.startswith(("beq cr6,", "bne cr6,", "b 0x"))]
        if address not in TAILS and actual_branches != BRANCHES[address]:
            raise ValueError(f"{address}: branch targets/order changed")
        calls = [part.split("0x", 1)[1].upper() for part in instructions
                 if part.startswith("bl 0x")]
        if calls != ([DIRECT_CALL[address]] if address in DIRECT_CALL else []):
            raise ValueError(f"{address}: dependency call changed")
        callee, vtable, inner, guard = parameters(address, instructions)
        rows.append(f"    {{0x{address.lower()}u, 0x{callee.lower()}u, "
                    f"{'true' if guard else 'false'}, 0x{vtable:08x}u, "
                    f"0x{inner:08x}u}},\n")
        entries.append({"address": address, "callee": callee,
                        "kind": ("callee" if address in CALLEES else
                                 "tail" if address in TAILS else "frame_wrapper"),
                        "frame_size": (112 if address in CALLEES - {"826DB6A8"}
                                       else 96 if address in WRAPPERS else 0),
                        "null_guard": guard, "wrapper_vtable": f"0x{vtable:08X}",
                        "wrapper_inner_vtable": f"0x{inner:08X}",
                        "source": raw.get("source", raw.get("generated_ppc_path")),
                        "source_line": raw.get("source_line", raw.get("line")),
                        "instruction_sequence": instructions,
                        "cfg_branches": raw.get("cfg_branches", actual_branches),
                        "direct_calls": calls,
                        "body": body})
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("composed parameter marker changed")
    before, remainder = source.split(BEGIN)
    _, after = remainder.split(END)
    source = before + BEGIN + "".join(rows) + END + after
    manifest = {"schema_version": 1, "entry_count": 11,
                "callee_count": 4, "tail_count": 4, "frame_wrapper_count": 3,
                "reused_lower": lower_evidence(),
                "entries": entries,
                "bounded_effects": "object/global/stack memory, full r3, LR/r30/r31 through own frames; lower helpers use previously verified semantic models; generic lower ABI, volatile GPR/CR and FP signaling NaN baseline difference excluded"}
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
    elif MANIFEST.read_text(encoding="utf-8") != encoded or \
            SOURCE.read_text(encoding="utf-8") != source:
        raise ValueError("composed manifest/source differs from exact PPC")
    print("PASS instance-composed 11 complete translated bodies/CFG")


if __name__ == "__main__":
    main()
