"""Recover the cached 52 singleton getters through one reviewed C++ operation.

Checks the full generated PPC body, including branch targets and instruction
constants, against the reviewed 19-instruction form. No source scan or hash.
"""

import argparse
import json
import re
from pathlib import Path
from string import Template


ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED REGISTERED GETTER PARAMETERS\n"
END = "    // END GENERATED REGISTERED GETTER PARAMETERS\n"

# The translated statements are checked as well as the assembly comments.
BODY = Template("""PPC_FUNC_IMPL(__imp__sub_$address) {
PPC_FUNC_PROLOGUE();
PPCRegister temp{};
// mflr r12
ctx.r12.u64 = ctx.lr;
// stw r12,-8(r1)
PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
// std r31,-16(r1)
PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
// stwu r1,-96(r1)
temp.u64 = ctx.r1.u64 + uint64_t(-96);
PPC_STORE_U32(temp.u32, ctx.r1.u32);
ctx.r1.u64 = temp.u64;
// lis r31,$global_hi
ctx.r31.s64 = $global_base;
// lwz r3,$global_lo(r31)
ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + $global_lo);
// cmplwi cr6,r3,0
ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
// bne cr6,0x$exit_lower
if (!ctx.cr6.eq) goto loc_$exit_upper;
// lis r11,$owner_hi
ctx.r11.s64 = $owner_base;
// addi r3,r11,$owner_lo
ctx.r3.s64 = ctx.r11.s64 + $owner_lo;
// bl 0x$constructor_lower
ctx.lr = 0x$constructor_return;
sub_$constructor(ctx, base);
// stw r3,$global_lo(r31)
PPC_STORE_U32(ctx.r31.u32 + $global_lo, ctx.r3.u32);
// bl 0x$registration_lower
ctx.lr = 0x$registration_return;
sub_$registration(ctx, base);
// lwz r3,$global_lo(r31)
ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + $global_lo);
loc_$exit_upper:
// addi r1,r1,96
ctx.r1.s64 = ctx.r1.s64 + 96;
// lwz r12,-8(r1)
ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
// mtlr r12
ctx.lr = ctx.r12.u64;
// ld r31,-16(r1)
ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
// blr
return;
}""")


def match_value(pattern, instruction, base=10):
    match = re.fullmatch(pattern, instruction)
    if match is None:
        raise ValueError(f"unexpected instruction: {instruction}")
    return int(match[1], base)


def recover(candidate, constructors):
    address = int(candidate["address"], 16)
    instructions = candidate["instructions"]
    if len(instructions) != 19:
        raise ValueError(f"instruction count changed: {address:08X}")
    global_hi = match_value(r"lis r31,(-?\d+)", instructions[4])
    global_lo = match_value(r"lwz r3,(-?\d+)\(r31\)", instructions[5])
    owner_hi = match_value(r"lis r11,(-?\d+)", instructions[8])
    owner_lo = match_value(r"addi r3,r11,(-?\d+)", instructions[9])
    constructor = match_value(r"bl 0x([0-9a-f]+)", instructions[10], 16)
    registration = match_value(r"bl 0x([0-9a-f]+)", instructions[12], 16)
    if constructors.get(f"{constructor:08X}", {}).get("kind") != "constructor":
        raise ValueError(f"unrecovered ordinary constructor: {constructor:08X}")
    expected = BODY.substitute(
        address=f"{address:08X}", global_hi=global_hi, global_lo=global_lo,
        global_base=global_hi << 16, owner_hi=owner_hi, owner_lo=owner_lo,
        owner_base=owner_hi << 16, exit_lower=f"{address + 56:08x}",
        exit_upper=f"{address + 56:08X}", constructor=f"{constructor:08X}",
        constructor_lower=f"{constructor:08x}",
        constructor_return=f"{address + 44:08X}",
        registration=f"{registration:08X}",
        registration_lower=f"{registration:08x}",
        registration_return=f"{address + 52:08X}")
    compact = lambda text: re.sub(r"\s+", "", text)
    if compact(expected) != compact(candidate["body"]):
        raise ValueError(f"translated body differs from reviewed form: {address:08X}")
    return {
        "address": f"{address:08X}",
        "source": candidate["generated_ppc_path"],
        "source_line": candidate["line"],
        "singleton_address": f"0x{((global_hi << 16) + global_lo) & 0xffffffff:08X}",
        "constructor": f"{constructor:08X}",
        "registration": f"{registration:08X}",
        "owner": f"0x{((owner_hi << 16) + owner_lo) & 0xffffffffffffffff:016X}",
        "frame_size": 96,
    }


def write_if_changed(path, text):
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidates", type=Path, default=ROOT /
                        "out/function-inventory/registered-dependency-candidates.json")
    args = parser.parse_args()
    candidates = json.loads(args.candidates.read_text(encoding="utf-8"))
    constructors = {entry["address"]: entry for entry in json.loads((ROOT /
        "LostOdysseyRecompSemantics/registered_constructor_families.json").read_text())[
            "entries"]}
    entries = sorted((recover(entry, constructors) for entry in candidates
                      if len(entry["instructions"]) == 19), key=lambda e: e["address"])
    if len(entries) != 52 or len({entry["address"] for entry in entries}) != 52:
        raise ValueError("reviewed getter set changed")
    manifest = {"schema_version": 1, "family": "registered_getter",
                "entry_count": len(entries), "entries": entries}
    write_if_changed(ROOT / "LostOdysseyRecompSemantics/registered_getter_families.json",
                     json.dumps(manifest, indent=2) + "\n")
    path = ROOT / "LostOdysseyRecompSemantics/src/registered_getter_family.cpp"
    source = path.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("C++ table markers missing or duplicated")
    prefix, tail = source.split(BEGIN, 1)
    _, suffix = tail.split(END, 1)
    rows = [f"    {{0x{e['address']}u, {e['singleton_address']}u, "
            f"0x{e['constructor']}u, 0x{e['registration']}u, {e['owner']}ull}},\n"
            for e in entries]
    write_if_changed(path, prefix + BEGIN + "".join(rows) + END + suffix)
    print(f"recovered {len(entries)} registered getter parameters")


if __name__ == "__main__":
    main()
