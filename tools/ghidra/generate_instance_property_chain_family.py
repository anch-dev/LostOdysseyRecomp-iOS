"""Strictly admit nine cached property initializers and their call graph."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_composed_initializer_family import expected_body
from generate_instance_vtable_family import raw_bodies, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "out/function-inventory"
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "instance_property_chain_families.json"
SOURCE = SEMANTICS / "src/instance_property_chain_family.cpp"
BEGIN = "    // BEGIN GENERATED INSTANCE PROPERTY CHAIN ENTRIES\n"
END = "    // END GENERATED INSTANCE PROPERTY CHAIN ENTRIES\n"
CALLEES = {"825D5398", "825AEEC0", "825AEB30", "826D6F28"}
TAILS = {"8245D2A8": "825D5398", "825AEC68": "825AEEC0",
         "825AEB20": "825AEB30", "826D6F18": "826D6F28"}
WRAPPER = "826D70A0"
CALLS = {"825D5398": ["82496948"],
         "825AEEC0": ["8229F678", "82496948"],
         "825AEB30": ["825AEEC0"],
         "826D6F28": ["825AEEC0"],
         WRAPPER: ["826D6F28"]}
FRAME_SIZE = {"825D5398": 96, "825AEEC0": 112,
              "825AEB30": 128, "826D6F28": 96, WRAPPER: 96}

# An immutable admission list is embedded, rather than choosing new shapes
# from the candidate cache on later invocations. Full translated statements
# and CFG are independently reconstructed below.
FIXED_INSTRUCTIONS = {
    # BEGIN FIXED PROPERTY CHAIN INSTRUCTIONS
    '8245D2A8': ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x825d5398'],
    '825AEB20': ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x825aeb30'],
    '825AEB30': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-128(r1)', 'mr r31,r3', 'bl 0x825aeec0', 'lis r11,-32226', 'mr r3,r31', 'addi r10,r11,-23352', 'li r11,0', 'stw r10,0(r31)', 'li r10,8', 'stw r11,84(r1)', 'stw r11,88(r1)', 'stw r11,92(r1)', 'stw r11,96(r1)', 'stw r11,100(r1)', 'stw r11,104(r1)', 'stw r11,148(r31)', 'stw r11,108(r1)', 'stw r11,80(r1)', 'rotlwi r11,r11,0', 'stw r11,152(r31)', 'lwz r11,84(r1)', 'stw r11,156(r31)', 'lwz r11,88(r1)', 'stw r11,160(r31)', 'lwz r11,92(r1)', 'stw r11,164(r31)', 'lwz r11,96(r1)', 'stw r11,168(r31)', 'lwz r11,100(r1)', 'stw r10,188(r31)', 'stw r11,172(r31)', 'lwz r11,104(r1)', 'stw r11,176(r31)', 'lwz r11,108(r1)', 'stw r11,180(r31)', 'li r11,0', 'stw r11,184(r31)', 'stw r11,192(r31)', 'stw r11,196(r31)', 'stw r11,200(r31)', 'stw r11,204(r31)', 'stw r10,208(r31)', 'stw r11,240(r31)', 'stw r11,244(r31)', 'stw r11,248(r31)', 'stw r11,256(r31)', 'stw r11,260(r31)', 'stw r11,264(r31)', 'stw r11,268(r31)', 'stw r10,272(r31)', 'stw r11,280(r31)', 'stw r11,284(r31)', 'stw r11,288(r31)', 'stw r11,292(r31)', 'stw r11,296(r31)', 'stw r11,300(r31)', 'stw r11,304(r31)', 'stw r10,308(r31)', 'stw r11,312(r31)', 'stw r11,316(r31)', 'stw r11,320(r31)', 'stw r11,340(r31)', 'stw r11,344(r31)', 'stw r11,348(r31)', 'stw r11,360(r31)', 'stw r11,364(r31)', 'stw r11,368(r31)', 'stw r11,372(r31)', 'stw r11,376(r31)', 'stw r11,380(r31)', 'addi r1,r1,128', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    '825AEC68': ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x825aeec0'],
    '825AEEC0': ['mflr r12', 'stw r12,-8(r1)', 'std r30,-24(r1)', 'std r31,-16(r1)', 'stwu r1,-112(r1)', 'lis r11,-32226', 'mr r31,r3', 'addi r10,r11,-23616', 'addi r30,r31,60', 'li r11,0', 'li r5,8', 'li r4,4', 'mr r3,r30', 'stw r10,0(r31)', 'stw r11,0(r30)', 'stw r11,4(r30)', 'stw r11,8(r30)', 'bl 0x8229f678', 'li r4,0', 'stw r31,12(r30)', 'addi r3,r31,76', 'bl 0x82496948', 'mr r3,r31', 'addi r1,r1,112', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r30,-24(r1)', 'ld r31,-16(r1)', 'blr'],
    '825D5398': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'lis r11,-32250', 'mr r31,r3', 'addi r10,r11,11456', 'lis r11,-32250', 'addi r3,r31,928', 'addi r9,r11,-3252', 'lis r11,-32250', 'stw r10,60(r31)', 'lis r10,-32229', 'addi r8,r11,-3260', 'li r11,0', 'addi r4,r10,-31792', 'stw r11,696(r31)', 'stw r11,700(r31)', 'stw r11,704(r31)', 'stw r11,712(r31)', 'stw r11,716(r31)', 'stw r11,720(r31)', 'stw r9,0(r31)', 'stw r8,60(r31)', 'bl 0x82496948', 'mr r3,r31', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    '826D6F18': ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x826d6f28'],
    '826D6F28': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3', 'bl 0x825aeec0', 'lis r11,-32224', 'mr r3,r31', 'addi r10,r11,10880', 'lis r11,-32224', 'addi r9,r11,10600', 'lis r11,-32224', 'stw r10,144(r31)', 'addi r8,r11,10880', 'li r11,0', 'stw r9,0(r31)', 'stw r8,144(r31)', 'stw r11,148(r31)', 'stw r11,164(r31)', 'stw r11,168(r31)', 'stw r11,172(r31)', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    '826D70A0': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3', 'cmplwi cr6,r31,0', 'beq cr6,0x826d70d8', 'bl 0x826d6f28', 'lis r11,-32224', 'lis r10,-32224', 'addi r11,r11,10912', 'addi r10,r10,11192', 'stw r11,0(r31)', 'stw r10,144(r31)', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    # END FIXED PROPERTY CHAIN INSTRUCTIONS
}


def exact_body(address: str, instructions: list[str]) -> list[str]:
    if address != "825AEB30":
        return expected_body(address, instructions)
    # The sole rotate-by-zero truncates to a PPC low word. Keep instruction
    # index/PC stable while using the existing complete-body translator.
    patched = ["mr r11,r11" if part == "rotlwi r11,r11,0" else part
               for part in instructions]
    if patched.count("mr r11,r11") != 1:
        raise ValueError("825AEB30 rotate count changed")
    translated = expected_body(address, patched)
    index = translated.index("// mr r11,r11")
    translated[index:index + 2] = ["// rotlwi r11,r11,0",
        "ctx.r11.u64 = __builtin_rotateleft32(ctx.r11.u32, 0);"]
    return translated


def foundation_proof() -> dict:
    path = SEMANTICS / "string_property_initializer_families.json"
    manifest = json.loads(path.read_text(encoding="utf-8"))
    entries = manifest.get("entries", [])
    addresses = {entry["address"] for entry in entries}
    if not {"822A06C0", "82496948"} <= addresses:
        raise ValueError("string property foundation mapping changed")
    return {"addresses": ["822A06C0", "82496948"],
            "implementation": "lo::semantic::gpu::string_property_initializer::Apply",
            "bounded_oracle":
            "tools/ghidra/test_semantic_string_property_initializer.py"}


def resize_proof() -> dict:
    recovery = json.loads((SEMANTICS / "recovery.json").read_text(
        encoding="utf-8"))
    matches = [entry for entry in recovery["functions"] if
               entry["address"] == "8229F678"]
    if len(matches) != 1 or matches[0]["name"] != \
            "lo::semantic::gpu::ResizeArray" or not all(
                matches[0]["stages"][stage] for stage in
                ("readable_implementation", "differential_validation")) or \
            not (ROOT / "tools/ghidra/test_semantic_allocation_array.py").is_file():
        raise ValueError("8229F678 recovered resize evidence changed")
    return {"address": "8229F678",
            "implementation": "lo::semantic::gpu::ResizeArray",
            "bounded_oracle": "tools/ghidra/test_semantic_allocation_array.py"}


def generate() -> tuple[dict, str]:
    callee_cache = json.loads((CACHE / "instance-tail-callees.json")
                              .read_text(encoding="utf-8"))
    callee_map = {entry["address"]: entry for entry in
                  callee_cache["callees"]}
    tail_map = {entry["address"]: entry for entry in
                callee_cache["parents"]}
    candidates = json.loads((CACHE / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    candidate_map = {entry["address"]: entry for entry in
                     candidates["entries"]}
    originals = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    wanted = CALLEES | set(TAILS) | {WRAPPER}
    if len(wanted) != 9 or set(FIXED_INSTRUCTIONS) != wanted:
        raise ValueError("property chain fixed target set changed")
    entries = []
    rows = []
    for address in sorted(wanted):
        cached = (callee_map[address] if address in CALLEES else
                  tail_map[address] if address in TAILS else
                  candidate_map[address])
        instructions = cached["instructions"]
        if instructions != FIXED_INSTRUCTIONS[address] or \
                len(instructions) != cached.get("instruction_count",
                                                len(instructions)):
            raise ValueError(f"{address}: fixed complete instruction sequence changed")
        body = cached["body"] if address in CALLEES else originals[address]
        if body != exact_body(address, instructions):
            raise ValueError(f"{address}: complete translated body/CFG changed")
        calls = [part.split("0x", 1)[1].upper() for part in instructions
                 if part.startswith("bl 0x")]
        if calls != CALLS.get(address, []):
            raise ValueError(f"{address}: direct call list changed")
        if address in TAILS and instructions != [
            "cmplwi cr6,r3,0", "beqlr cr6",
            f"b 0x{TAILS[address].lower()}"]:
            raise ValueError(f"{address}: null-tail target changed")
        kind = ("callee" if address in CALLEES else "null_tail" if
                address in TAILS else "frame_wrapper")
        target = TAILS.get(address, address)
        rows.append(f"    {{0x{address.lower()}u, 0x{target.lower()}u, "
                    f"{'true' if address in TAILS else 'false'}}},\n")
        entries.append({"address": address, "kind": kind,
                        "callee": target, "frame_size": FRAME_SIZE.get(address, 0),
                        "null_guard": address in TAILS or address == WRAPPER,
                        "source": cached.get("source", cached.get(
                            "generated_ppc_path")),
                        "source_line": cached.get("source_line",
                                                  cached.get("line")),
                        "instruction_sequence": instructions,
                        "cfg_branches": [part for part in instructions if
                                         part.startswith(("beq cr6,", "b 0x"))],
                        "direct_calls": calls, "body": body})
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("property chain generated table marker changed")
    before, tail = source.split(BEGIN)
    _, after = tail.split(END)
    source = before + BEGIN + "".join(rows) + END + after
    manifest = {"schema_version": 1, "entry_count": 9,
                "callee_count": 4, "tail_count": 4, "wrapper_count": 1,
                "reused_foundation": foundation_proof(),
                "reused_resize": resize_proof(),
                "entries": entries,
                "bounded_effects": "ordered ordinary object/global/stack memory, full r3 and r4 into foundation, LR/r28-r31 live frames; generic helper ABI, volatile GPR/CR and exact atomic/MMIO stack U64 effects excluded"}
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
        raise ValueError("property chain manifest/source differs from cached PPC")
    print("PASS instance-property-chain 9 complete translated bodies/CFG")


if __name__ == "__main__":
    main()
