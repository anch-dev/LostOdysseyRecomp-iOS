"""Validate the four complete UI fill initializer bodies from the cached inventory."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
BEGIN = "    // BEGIN GENERATED INSTANCE FILL PARAMETERS\n"
END = "    // END GENERATED INSTANCE FILL PARAMETERS\n"
TARGETS = {
    "8264E920": (60, -15968, False),
    "82667430": (72, 3976, False),
    "826674B0": (72, 4624, True),
    "82667568": (72, 4352, True),
}


def checked_spec(entry: dict, body: list[str]) -> dict:
    address = entry["address"]
    offset, low, framed = TARGETS[address]
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{address}) {{", "PPC_FUNC_PROLOGUE();"]
    instructions = []

    def ins(comment: str, *cpp: str) -> None:
        instructions.append(comment)
        expected.extend(["// " + comment, *cpp])

    if framed:
        branch = f"{int(address, 16) + 0x38:08X}"
        expected.append("PPCRegister temp{};")
        ins("mflr r12", "ctx.r12.u64 = ctx.lr;")
        ins("stw r12,-8(r1)", "PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);")
        ins("std r31,-16(r1)", "PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);")
        ins("stwu r1,-96(r1)", "temp.u64 = ctx.r1.u64 + uint64_t(-96);",
            "PPC_STORE_U32(temp.u32, ctx.r1.u32);", "ctx.r1.u64 = temp.u64;")
        ins("mr r31,r3", "ctx.r31.u64 = ctx.r3.u64;")
        ins("cmplwi cr6,r31,0", "ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);")
        ins(f"beq cr6,0x{branch.lower()}", f"if (ctx.cr6.eq) goto loc_{branch};")
        ins(f"addi r3,r31,{offset}", f"ctx.r3.s64 = ctx.r31.s64 + {offset};")
        ins("li r5,100", "ctx.r5.s64 = 100;")
        ins("li r4,0", "ctx.r4.s64 = 0;")
        ins("bl 0x82b7bc40", f"ctx.lr = 0x{int(address, 16) + 0x2c:08X};",
            "sub_82B7BC40(ctx, base);")
        ins("lis r11,-32225", "ctx.r11.s64 = -2111897600;")
        ins(f"addi r11,r11,{low}", f"ctx.r11.s64 = ctx.r11.s64 + {low};")
        ins("stw r11,0(r31)", "PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r11.u32);")
        expected.append(f"loc_{branch}:")
        ins("addi r1,r1,96", "ctx.r1.s64 = ctx.r1.s64 + 96;")
        ins("lwz r12,-8(r1)", "ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);")
        ins("mtlr r12", "ctx.lr = ctx.r12.u64;")
        ins("ld r31,-16(r1)", "ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);")
        ins("blr", "return;")
        cfg, calls = [[6, "beq", 14]], ["82B7BC40"]
    else:
        ins("mr r11,r3", "ctx.r11.u64 = ctx.r3.u64;")
        ins("cmplwi cr6,r11,0", "ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);")
        ins("beqlr cr6", "if (ctx.cr6.eq) return;")
        ins("lis r10,-32225", "ctx.r10.s64 = -2111897600;")
        ins(f"addi r3,r11,{offset}", f"ctx.r3.s64 = ctx.r11.s64 + {offset};")
        ins(f"addi r10,r10,{low}", f"ctx.r10.s64 = ctx.r10.s64 + {low};")
        ins("li r5,100", "ctx.r5.s64 = 100;")
        ins("li r4,0", "ctx.r4.s64 = 0;")
        ins("stw r10,0(r11)", "PPC_STORE_U32(ctx.r11.u32 + 0, ctx.r10.u32);")
        ins("b 0x82b7bc40", "sub_82B7BC40(ctx, base);", "return;")
        cfg, calls = [], []
    expected.append("}")
    if entry["instructions"] != instructions or entry["cfg_branches"] != cfg or \
            entry["direct_calls"] != calls or body != expected:
        raise ValueError(f"complete UI fill body/CFG changed: {address}")
    return {"address": address, "vtable": signed_word(-32225, low, address),
            "field_offset": offset, "framed": framed,
            "source": entry["generated_ppc_path"], "source_line": entry["line"],
            "registered_types": entry["types"]}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidates", type=Path, default=ROOT /
                        "out/function-inventory/registered-instance-candidates.json")
    parser.add_argument("--originals", type=Path, default=ROOT /
                        "out/function-inventory/registered-instance-originals.cpp.gz")
    parser.add_argument("--manifest", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/instance_fill_initializer_families.json")
    parser.add_argument("--source", type=Path, default=ROOT /
                        "LostOdysseyRecompSemantics/src/instance_fill_initializer_family.cpp")
    args = parser.parse_args()
    candidates = json.loads(args.candidates.read_text(encoding="utf-8"))
    by_address = {entry["address"]: entry for entry in candidates["entries"]}
    bodies = raw_bodies(args.originals)
    if len(by_address) != 786 or len(bodies) != 786:
        raise ValueError("cached instance inventory changed")
    entries = [checked_spec(by_address[a], bodies[a]) for a in TARGETS]
    manifest = {"schema_version": 1, "family": "instance_fill_initializer",
                "entry_count": len(entries), "fill_target": "82B7BC40",
                "fill_bytes": 100, "fill_value": 0, "frame_bytes": 96,
                "entries": entries}
    write_if_changed(args.manifest, json.dumps(manifest, indent=2) + "\n")
    source = args.source.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("UI fill parameter markers changed")
    prefix, rest = source.split(BEGIN)
    _, suffix = rest.split(END)
    rows = "".join(f"    {{0x{e['address'].lower()}u, {e['vtable'].lower()}u, "
                   f"{e['field_offset']}u, {str(e['framed']).lower()}}},\n" for e in entries)
    write_if_changed(args.source, prefix + BEGIN + rows + END + suffix)
    print(f"generated {len(entries)} exact UI fill initializers")


if __name__ == "__main__":
    main()
