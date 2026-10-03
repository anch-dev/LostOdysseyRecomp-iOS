"""Admit eight exact cached owned-state callers and their parameters."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_composed_initializer_family import expected_body
from generate_instance_vtable_family import raw_bodies, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "out/function-inventory"
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "instance_owned_state_families.json"
SOURCE = SEMANTICS / "src/instance_owned_state_family.cpp"
BEGIN = "    // BEGIN GENERATED INSTANCE OWNED STATE ENTRIES\n"
END = "    // END GENERATED INSTANCE OWNED STATE ENTRIES\n"
CALLEES = {"827134B8", "82693E38"}
TAILS = {"82713418": "827134B8", "82693DD8": "82693E38"}
CONDITIONAL_TAIL = "82693A10"
WRAPPERS = {"82693C38": "823FA008", "82713538": "827134B8",
            "8272FE20": "827134B8"}
CALLS = {"827134B8": ["823FA008"], "82693E38": ["823FA008"],
         "82693C38": ["823FA008"], "82713538": ["827134B8"],
         "8272FE20": ["827134B8"]}

# These complete sequences are fixed review admission, not selected anew by
# scanning the cache. Every translated C++ statement and CFG label is checked.
FIXED_INSTRUCTIONS = {
    # BEGIN FIXED OWNED STATE INSTRUCTIONS
    '82693A10': ['cmplwi cr6,r3,0', 'beqlr cr6', 'lis r11,-32256', 'ld r10,8(r3)', 'addi r11,r11,17384', 'rlwinm r10,r10,0,22,22', 'cmpldi cr6,r10,0', 'stw r11,0(r3)', 'bnelr cr6', 'b 0x823fa008'],
    '82693C38': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3', 'cmplwi cr6,r31,0', 'beq cr6,0x82693c80', 'lis r11,-32256', 'ld r10,8(r31)', 'addi r11,r11,17384', 'rlwinm r10,r10,0,22,22', 'cmpldi cr6,r10,0', 'stw r11,0(r31)', 'bne cr6,0x82693c74', 'bl 0x823fa008', 'lis r11,-32256', 'addi r11,r11,17672', 'stw r11,0(r31)', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    '82693DD8': ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x82693e38'],
    '82693E38': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3', 'lis r11,-32256', 'addi r11,r11,17384', 'ld r10,8(r31)', 'rlwinm r10,r10,0,22,22', 'stw r11,0(r31)', 'cmpldi cr6,r10,0', 'bne cr6,0x82693e6c', 'bl 0x823fa008', 'lis r10,-32250', 'lis r11,-32256', 'addi r9,r10,11456', 'lis r10,-32224', 'mr r3,r31', 'addi r8,r10,-26760', 'lis r10,-32256', 'lfs f0,3664(r11)', 'stw r9,72(r31)', 'li r9,8', 'addi r7,r10,17960', 'lis r10,-32256', 'addi r11,r31,172', 'stw r8,76(r31)', 'addi r6,r10,4000', 'lis r10,-32224', 'stw r7,0(r31)', 'addi r5,r10,-16472', 'li r10,0', 'stw r6,72(r31)', 'stw r5,76(r31)', 'stw r9,132(r31)', 'stw r10,116(r31)', 'stw r10,120(r31)', 'stw r10,124(r31)', 'stw r10,128(r31)', 'stw r9,168(r31)', 'stw r10,152(r31)', 'stw r10,156(r31)', 'stw r10,160(r31)', 'stw r10,164(r31)', 'stfs f0,8(r11)', 'lwz r9,12(r11)', 'stfs f0,24(r11)', 'lwz r8,28(r11)', 'stfs f0,40(r11)', 'lwz r7,44(r11)', 'oris r9,r9,32768', 'lwz r6,60(r11)', 'oris r8,r8,32768', 'oris r7,r7,32768', 'stfs f0,56(r11)', 'oris r6,r6,32768', 'stw r10,0(r11)', 'stw r10,4(r11)', 'stw r9,12(r11)', 'stw r10,16(r11)', 'stw r10,20(r11)', 'stw r8,28(r11)', 'stw r10,32(r11)', 'stw r10,36(r11)', 'stw r7,44(r11)', 'stw r10,48(r11)', 'stw r10,52(r11)', 'stw r6,60(r11)', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    '82713418': ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x827134b8'],
    '827134B8': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3', 'lis r11,-32256', 'addi r11,r11,17384', 'ld r10,8(r31)', 'rlwinm r10,r10,0,22,22', 'stw r11,0(r31)', 'cmpldi cr6,r10,0', 'bne cr6,0x827134ec', 'bl 0x823fa008', 'lis r11,-32223', 'li r9,8', 'addi r10,r11,-16232', 'li r11,0', 'mr r3,r31', 'stw r10,0(r31)', 'stw r11,108(r31)', 'stw r11,112(r31)', 'stw r11,116(r31)', 'stw r11,120(r31)', 'stw r9,124(r31)', 'stw r11,128(r31)', 'stw r11,132(r31)', 'stw r11,136(r31)', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    '82713538': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3', 'cmplwi cr6,r31,0', 'beq cr6,0x82713564', 'bl 0x827134b8', 'lis r11,-32256', 'addi r11,r11,20112', 'stw r11,0(r31)', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    '8272FE20': ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3', 'cmplwi cr6,r31,0', 'beq cr6,0x8272fe4c', 'bl 0x827134b8', 'lis r11,-32223', 'addi r11,r11,8232', 'stw r11,0(r31)', 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    # END FIXED OWNED STATE INSTRUCTIONS
}


def exact_body(address: str, instructions: list[str]) -> list[str]:
    patched = []
    for part in instructions:
        if part.startswith("oris "):
            register = part.split(" ", 1)[1].split(",", 1)[0]
            if part != f"oris {register},{register},32768":
                raise ValueError(f"{address}: unexpected oris parameter")
            patched.append(f"mr {register},{register}")
        elif part == "bnelr cr6":
            patched.append("beqlr cr6")
        else:
            patched.append(part)
    result = expected_body(address, patched)
    for part in instructions:
        if part.startswith("oris "):
            register = part.split(" ", 1)[1].split(",", 1)[0]
            marker = f"// mr {register},{register}"
            index = result.index(marker)
            result[index:index + 2] = [f"// {part}",
                f"ctx.{register}.u64 = ctx.{register}.u64 | 2147483648;"]
        elif part == "bnelr cr6":
            index = len(result) - 1 - result[::-1].index("// beqlr cr6")
            result[index:index + 2] = ["// bnelr cr6",
                                          "if (!ctx.cr6.eq) return;"]
    return result


def foundation_proof() -> dict:
    manifest = json.loads((SEMANTICS / "owned_state_initializer_families.json")
                          .read_text(encoding="utf-8"))
    if {entry["address"] for entry in manifest["entries"]} != \
            {"823FA008", "82406A38"}:
        raise ValueError("owned-state foundation address set changed")
    return {"address": "823FA008",
            "implementation": "lo::semantic::gpu::owned_state_initializer::Apply",
            "bounded_oracle": "tools/ghidra/test_semantic_owned_state_initializer.py",
            "destructor": "StateServices::DestroyState callback boundary"}


def generate() -> tuple[dict, str]:
    cache = json.loads((CACHE / "owned-state-closure-candidates.json")
                       .read_text(encoding="utf-8"))
    candidates = {entry["address"]: entry for entry in cache}
    originals = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    wanted = CALLEES | set(TAILS) | {CONDITIONAL_TAIL} | set(WRAPPERS)
    if len(wanted) != 8 or set(FIXED_INSTRUCTIONS) != wanted or \
            not wanted <= set(candidates):
        raise ValueError("owned-state target set changed")
    entries = []
    rows = []
    for address in sorted(wanted):
        cached = candidates[address]
        instructions = cached["instructions"]
        body = cached.get("body") or originals[address]
        if instructions != FIXED_INSTRUCTIONS[address] or \
                cached["instruction_count"] != len(instructions):
            raise ValueError(f"{address}: fixed instruction sequence changed")
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
        if address == CONDITIONAL_TAIL and instructions[-2:] != [
            "bnelr cr6", "b 0x823fa008"]:
            raise ValueError("conditional tail state target changed")
        if address in CALLEES:
            kind, callee, guard = "callee", address, False
        elif address in TAILS:
            kind, callee, guard = "null_tail", TAILS[address], True
        elif address == CONDITIONAL_TAIL:
            kind, callee, guard = "conditional_tail", address, True
        else:
            kind, callee, guard = "frame_wrapper", address, False
        rows.append(f"    {{0x{address.lower()}u, 0x{callee.lower()}u, "
                    f"{'true' if guard else 'false'}}},\n")
        branches = [part for part in instructions if
                    part.startswith(("beq cr6,", "bne cr6,", "b 0x")) or
                    part in ("beqlr cr6", "bnelr cr6")]
        entries.append({"address": address, "kind": kind, "callee": callee,
                        "frame_size": 96 if address in CALLEES or
                                     address in WRAPPERS else 0,
                        "null_guard": guard or address in WRAPPERS,
                        "source": cached.get("source", cached.get(
                            "generated_ppc_path")),
                        "source_line": cached.get("source_line",
                                                  cached.get("line")),
                        "instruction_sequence": instructions,
                        "cfg_branches": branches, "direct_calls": calls,
                        "body": body})
    source = SOURCE.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("owned-state source entry marker changed")
    before, tail = source.split(BEGIN)
    _, after = tail.split(END)
    source = before + BEGIN + "".join(rows) + END + after
    manifest = {"schema_version": 1, "entry_count": 8,
                "callee_count": 2, "null_tail_count": 2,
                "conditional_tail_count": 1, "frame_wrapper_count": 3,
                "reused_foundation": foundation_proof(), "entries": entries,
                "bounded_effects": "ordered ordinary object/global/stack memory, full r3, LR/r31 own frames, first lfs/F0 and FPSCR flush callback; destructor is an explicit external callback, generic lower ABI and volatile GPR/CR excluded; optimized generated baseline known sNaN mismatch"}
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
        raise ValueError("owned-state manifest/source differs from cached PPC")
    print("PASS instance-owned-state 8 complete translated bodies/CFG")


if __name__ == "__main__":
    main()
