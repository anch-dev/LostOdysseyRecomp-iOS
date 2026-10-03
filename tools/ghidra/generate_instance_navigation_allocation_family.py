"""Admit five exact navigation allocation bodies and their call parameters."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

from generate_instance_composed_initializer_family import expected_body
from generate_instance_vtable_family import raw_bodies, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "out/function-inventory"
SEMANTICS = ROOT / "LostOdysseyRecompSemantics"
MANIFEST = SEMANTICS / "instance_navigation_allocation_families.json"
ADDRESSES = ("823B8728", "825AEF38", "825AF048", "825E12A0", "825E2B08")
SOURCES = {
    "823B8728": ("allocation-composed-link.cpp",
                 "LostOdysseyRecompLib/ppc/ppc_recomp.12.cpp", 9770),
    "825AEF38": ("allocation-composed-nested.cpp",
                 "LostOdysseyRecompLib/ppc/ppc_recomp.42.cpp", 29491),
    "825AF048": ("allocation-composed-foundations.cpp",
                 "LostOdysseyRecompLib/ppc/ppc_recomp.42.cpp", 29650),
}
CALLS = {
    "823B8728": [], "825AEF38": ["823B8728"],
    "825AF048": ["825AEF38"], "825E12A0": ["82486C88", "825AF048"],
    "825E2B08": [],
}

# Immutable instruction admission. The complete translated statement stream
# is reconstructed independently below, including labels and call-site LR.
FIXED_INSTRUCTIONS = {
    # BEGIN FIXED NAVIGATION ALLOCATION INSTRUCTIONS
    "823B8728": ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3',
 'lwz r11,16(r31)', 'rlwinm r11,r11,0,0,0', 'cmplwi cr6,r11,0', 'bne cr6,0x823b87d0',
 'stw r11,8(r31)', 'lis r11,-31945', 'li r10,0', 'stw r31,4(r31)', 'addi r11,r11,-12796',
 'addi r9,r31,4', 'stw r10,12(r31)', 'lwz r10,0(r11)', 'cmplwi cr6,r10,0',
 'beq cr6,0x823b8780', 'addi r8,r9,4', 'stw r8,8(r10)', 'lwz r10,0(r11)', 'stw r11,8(r9)',
 'stw r10,4(r9)', 'stw r9,0(r11)', 'lis r11,-31950', 'lwz r11,-32656(r11)',
 'cmpwi cr6,r11,0', 'beq cr6,0x823b87c4', 'lwz r11,0(r31)', 'mr r3,r31', 'lwz r11,0(r11)',
 'mtctr r11', 'bctrl', 'lwz r11,0(r31)', 'mr r3,r31', 'lwz r11,8(r11)', 'mtctr r11',
 'bctrl', 'lwz r11,16(r31)', 'oris r11,r11,32768', 'stw r11,16(r31)', 'addi r1,r1,96',
 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    "825AEF38": ['mflr r12', 'stw r12,-8(r1)', 'std r31,-16(r1)', 'stwu r1,-96(r1)', 'mr r31,r3',
 'li r11,0', 'lis r10,-32226', 'li r9,8', 'addi r10,r10,-22620', 'stw r11,8(r31)',
 'stw r11,12(r31)', 'lwz r8,16(r31)', 'stw r10,0(r31)', 'clrlwi r10,r8,1',
 'stw r10,16(r31)', 'stw r11,20(r31)', 'stw r11,24(r31)', 'stw r11,28(r31)',
 'stw r11,32(r31)', 'stw r9,36(r31)', 'stw r11,40(r31)', 'stw r11,44(r31)',
 'stw r11,48(r31)', 'stw r11,52(r31)', 'stw r9,56(r31)', 'bl 0x823b8728', 'mr r3,r31',
 'addi r1,r1,96', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r31,-16(r1)', 'blr'],
    "825AF048": ['mflr r12', 'bl 0x82b7a6e4', 'stwu r1,-128(r1)', 'lis r11,-32249', 'lis r10,-32226',
 'lis r9,-32226', 'mr r31,r3', 'li r30,0', 'addi r11,r11,18220', 'addi r10,r10,-22592',
 'addi r9,r9,-22584', 'addi r29,r31,16', 'stw r30,4(r31)', 'li r28,1', 'stw r30,8(r31)',
 'mr r27,r29', 'stw r11,12(r31)', 'stw r10,0(r31)', 'stw r9,12(r31)', 'mr r3,r27',
 'bl 0x825aef38', 'addi r28,r28,-1', 'addi r27,r27,60', 'cmpwi cr6,r28,0',
 'bge cr6,0x825af094', 'lis r11,-32256', 'stw r30,412(r31)', 'addi r10,r31,76',
 'stw r30,416(r31)', 'stw r30,420(r31)', 'lfs f0,3664(r11)', 'lis r11,-32231',
 'lfs f13,-27252(r11)', 'li r11,8', 'stw r11,424(r31)', 'stw r30,428(r31)',
 'stw r30,432(r31)', 'stw r30,436(r31)', 'stw r11,440(r31)', 'stfs f0,400(r31)',
 'stfs f13,404(r31)', 'stw r29,136(r31)', 'stw r10,140(r31)', 'stfs f13,144(r31)',
 'stfs f0,148(r31)', 'stfs f0,152(r31)', 'stfs f0,156(r31)', 'stfs f0,160(r31)',
 'stfs f13,164(r31)', 'stfs f0,168(r31)', 'stfs f0,172(r31)', 'stfs f0,176(r31)',
 'stfs f0,180(r31)', 'stfs f13,184(r31)', 'stfs f0,188(r31)', 'stfs f0,192(r31)',
 'stfs f0,196(r31)', 'stfs f0,200(r31)', 'stfs f13,204(r31)', 'stfs f13,208(r31)',
 'stfs f0,212(r31)', 'stfs f0,216(r31)', 'stfs f0,220(r31)', 'stfs f0,224(r31)',
 'stfs f13,228(r31)', 'stfs f0,232(r31)', 'stfs f0,236(r31)', 'stfs f0,240(r31)',
 'stfs f0,244(r31)', 'stfs f13,248(r31)', 'stfs f0,252(r31)', 'stfs f0,256(r31)',
 'stfs f0,260(r31)', 'stfs f0,264(r31)', 'stfs f13,268(r31)', 'stfs f13,272(r31)',
 'stfs f0,276(r31)', 'stfs f0,280(r31)', 'stfs f0,284(r31)', 'stfs f0,288(r31)',
 'stfs f13,292(r31)', 'stfs f0,296(r31)', 'stfs f0,300(r31)', 'stfs f0,304(r31)',
 'stfs f0,308(r31)', 'stfs f13,312(r31)', 'stfs f0,316(r31)', 'stfs f0,320(r31)',
 'stfs f0,324(r31)', 'stfs f0,328(r31)', 'stfs f13,332(r31)', 'stfs f13,336(r31)',
 'stfs f0,340(r31)', 'stfs f0,344(r31)', 'stfs f0,348(r31)', 'stfs f0,352(r31)',
 'stfs f13,356(r31)', 'stfs f0,360(r31)', 'stfs f0,364(r31)', 'stfs f0,368(r31)',
 'stfs f0,372(r31)', 'stfs f13,376(r31)', 'stfs f0,380(r31)', 'stfs f0,384(r31)',
 'stfs f0,388(r31)', 'stfs f0,392(r31)', 'stfs f13,396(r31)', 'mr r3,r31',
 'stw r30,408(r31)', 'addi r1,r1,128', 'b 0x82b7a734'],
    "825E12A0": ['mflr r12', 'stw r12,-8(r1)', 'std r30,-24(r1)', 'std r31,-16(r1)', 'stwu r1,-112(r1)',
 'lis r11,-32250', 'lis r10,-32226', 'lis r9,-32256', 'mr r31,r3', 'addi r11,r11,11456',
 'addi r10,r10,-6336', 'addi r9,r9,4004', 'li r30,0', 'stw r11,60(r31)', 'mr r11,r31',
 'stw r10,0(r31)', 'stw r9,60(r31)', 'stw r30,128(r31)', 'stw r30,132(r31)',
 'ld r10,8(r11)', 'rlwinm r10,r10,0,21,22', 'cmpldi cr6,r10,0', 'bne cr6,0x825e1364',
 'lwz r11,40(r11)', 'cmplwi cr6,r11,0', 'bne cr6,0x825e12ec', 'li r3,448',
 'bl 0x82486c88', 'cmplwi cr6,r3,0', 'beq cr6,0x825e1320', 'bl 0x825af048',
 'b 0x825e1324', 'mr r3,r30', 'lwz r11,120(r31)', 'stw r3,124(r31)', 'cmplwi cr6,r11,0',
 'bne cr6,0x825e1344', 'lis r11,-31951', 'lwz r11,24500(r11)', 'lwz r11,560(r11)',
 'stw r11,120(r31)', 'lis r11,-31950', 'lwz r3,-32624(r11)', 'lwz r11,0(r3)',
 'lwz r11,0(r11)', 'mtctr r11', 'bctrl', 'mr r11,r3', 'stw r11,132(r31)', 'mr r3,r31',
 'addi r1,r1,112', 'lwz r12,-8(r1)', 'mtlr r12', 'ld r30,-24(r1)', 'ld r31,-16(r1)',
 'blr'],
    "825E2B08": ['cmplwi cr6,r3,0', 'beqlr cr6', 'b 0x825e12a0'],
    # END FIXED NAVIGATION ALLOCATION INSTRUCTIONS
}


def cached_body(address: str, original: dict[str, list[str]],
                tail: dict) -> tuple[str, str, int]:
    if address in SOURCES:
        name, path, line = SOURCES[address]
        lines = (CACHE / name).read_text(encoding="utf-8").splitlines()
        marker = f"PPC_FUNC_IMPL(__imp__sub_{address}) {{"
        if lines.count(marker) != 1:
            raise ValueError(f"{address}: cached body missing or duplicate")
        start = lines.index(marker)
        end = next((i for i in range(start + 1, len(lines))
                    if lines[i].strip() == "}"), None)
        if end is None:
            raise ValueError(f"{address}: cached body unterminated")
        return "\n".join(lines[start:end + 1]), path, line
    if address == "825E12A0":
        entry = next(e for e in tail["callees"] if e["address"] == address)
        return "\n".join(entry["body"]), entry["source"], entry["source_line"]
    entry = next(e for e in tail["parents"] if e["address"] == address)
    return "\n".join(original[address]), entry["source"], entry["source_line"]


def reconstruct(address: str, instructions: list[str]) -> list[str]:
    patched = []
    replacements = []
    for index, instruction in enumerate(instructions):
        pc = int(address, 16) + index * 4
        if instruction == "rlwinm r11,r11,0,0,0":
            placeholder = "mr r11,r11"
            translated = ["ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | "
                          "(ctx.r11.u64 << 32), 0) & 0x80000000;"]
        elif instruction == "rlwinm r10,r10,0,21,22":
            placeholder = "mr r10,r10"
            translated = ["ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | "
                          "(ctx.r10.u64 << 32), 0) & 0x600;"]
        elif instruction == "clrlwi r10,r8,1":
            placeholder = "mr r10,r8"
            translated = ["ctx.r10.u64 = ctx.r8.u32 & 0x7FFFFFFF;"]
        elif instruction == "oris r11,r11,32768":
            placeholder = "mr r11,r11"
            translated = ["ctx.r11.u64 = ctx.r11.u64 | 2147483648;"]
        elif instruction == "mtctr r11":
            placeholder = "mr r11,r11"
            translated = ["ctx.ctr.u64 = ctx.r11.u64;"]
        elif instruction == "bctrl":
            placeholder = "mr r11,r11"
            translated = [f"ctx.lr = 0x{pc + 4:08X};",
                          "PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);"]
        elif instruction == "bge cr6,0x825af094":
            placeholder = "bne cr6,0x825af094"
            translated = ["if (!ctx.cr6.lt) goto loc_825AF094;"]
        elif instruction == "bl 0x82b7a6e4":
            placeholder = instruction
            translated = [f"ctx.lr = 0x{pc + 4:08X};",
                          "__savegprlr_27(ctx, base);"]
        elif instruction == "b 0x82b7a734":
            placeholder = instruction
            translated = ["__restgprlr_27(ctx, base);", "return;"]
        else:
            patched.append(instruction)
            continue
        patched.append(placeholder)
        replacements.append((placeholder, instruction, translated))
    built = expected_body(address, patched)
    cursor = 0
    for placeholder, instruction, translated in replacements:
        marker = f"// {placeholder}"
        index = built.index(marker, cursor)
        old_length = 3 if placeholder.startswith(("bl ", "b ")) else 2
        built[index:index + old_length] = [f"// {instruction}", *translated]
        cursor = index + 1 + len(translated)
    return built


def generate() -> dict:
    original = raw_bodies(CACHE / "registered-instance-originals.cpp.gz")
    tail = json.loads((CACHE / "instance-tail-callees.json")
                      .read_text(encoding="utf-8"))
    if set(FIXED_INSTRUCTIONS) != set(ADDRESSES):
        raise ValueError("navigation target set changed")
    entries = []
    for address in ADDRESSES:
        body, source, line = cached_body(address, original, tail)
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", body, re.M)
        if instructions != FIXED_INSTRUCTIONS[address]:
            raise ValueError(f"{address}: fixed instruction sequence changed")
        normalized = [part.strip() for part in body.splitlines() if part.strip()]
        if normalized != reconstruct(address, instructions):
            raise ValueError(f"{address}: complete translated body/CFG changed")
        calls = [part.split("0x", 1)[1].upper() for part in instructions
                 if part.startswith("bl 0x") and not part.startswith("bl 0x82b7")]
        if calls != CALLS[address]:
            raise ValueError(f"{address}: direct call order changed")
        if address == "825E2B08" and instructions != [
                "cmplwi cr6,r3,0", "beqlr cr6", "b 0x825e12a0"]:
            raise ValueError("navigation parent null tail changed")
        entries.append({"address": address, "source": source,
                        "source_line": line, "instructions": instructions,
                        "translated_body": body,
                        "direct_calls": calls,
                        "cfg": [part for part in instructions if
                                part.startswith(("beq", "bne", "bge", "b 0x"))]})
    return {"schema_version": 1, "entry_count": 5,
            "new_helper_count": 3, "navigation_parent_count": 2,
            "entries": entries,
            "bounded_effects": "ordered ordinary guest memory, full r3/SP, "
            "r27-r31/LR own frames, first lfs/F0/F13 and FPSCR flush; "
            "three dynamic vtable call sites use an explicit external "
            "service; lower allocation evidence reused; generic helper ABI "
            "and optimized sNaN baseline excluded"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    encoded = json.dumps(generate(), indent=2) + "\n"
    if args.write:
        write_if_changed(MANIFEST, encoded)
    elif MANIFEST.read_text(encoding="utf-8") != encoded:
        raise ValueError("navigation allocation manifest changed")
    print("PASS navigation allocation 5 complete translated bodies/CFG")


if __name__ == "__main__":
    main()
