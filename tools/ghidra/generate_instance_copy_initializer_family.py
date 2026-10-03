"""Validate the complete SceneCapture2D copy initializer against its cached PPC."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from generate_instance_vtable_family import raw_bodies, signed_word, write_if_changed

ROOT = Path(__file__).resolve().parents[2]
ADDRESS = "8268CC28"
BEGIN = "// BEGIN GENERATED INSTANCE COPY PARAMETERS\n"
END = "// END GENERATED INSTANCE COPY PARAMETERS\n"


def checked_spec(entry: dict, body: list[str]) -> dict:
    expected = [f"PPC_FUNC_IMPL(__imp__sub_{ADDRESS}) {{",
                "PPC_FUNC_PROLOGUE();", "PPCRegister temp{};"]
    instructions = []

    def ins(comment: str, *cpp: str) -> None:
        instructions.append(comment)
        expected.extend(["// " + comment, *cpp])

    ins("mflr r12", "ctx.r12.u64 = ctx.lr;")
    ins("stw r12,-8(r1)", "PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);")
    ins("std r30,-24(r1)", "PPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);")
    ins("std r31,-16(r1)", "PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);")
    ins("stwu r1,-112(r1)", "temp.u64 = ctx.r1.u64 + uint64_t(-112);",
        "PPC_STORE_U32(temp.u32, ctx.r1.u32);", "ctx.r1.u64 = temp.u64;")
    ins("mr r31,r3", "ctx.r31.u64 = ctx.r3.u64;")
    ins("cmplwi cr6,r31,0", "ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);")
    ins("beq cr6,0x8268cc88", "if (ctx.cr6.eq) goto loc_8268CC88;")
    ins("lis r10,-31964", "ctx.r10.s64 = -2094792704;")
    ins("lwz r9,120(r31)", "ctx.r9.u64 = PPC_LOAD_U32(ctx.r31.u32 + 120);")
    ins("lis r11,-32224", "ctx.r11.s64 = -2111832064;")
    ins("addi r30,r10,-23264", "ctx.r30.s64 = ctx.r10.s64 + -23264;")
    ins("addi r11,r11,-22080", "ctx.r11.s64 = ctx.r11.s64 + -22080;")
    ins("oris r10,r9,32768", "ctx.r10.u64 = ctx.r9.u64 | 2147483648;")
    ins("addi r3,r31,144", "ctx.r3.s64 = ctx.r31.s64 + 144;")
    ins("mr r4,r30", "ctx.r4.u64 = ctx.r30.u64;")
    ins("li r5,64", "ctx.r5.s64 = 64;")
    ins("stw r11,0(r31)", "PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r11.u32);")
    ins("stw r10,120(r31)", "PPC_STORE_U32(ctx.r31.u32 + 120, ctx.r10.u32);")
    ins("bl 0x82b7a0b0", "ctx.lr = 0x8268CC78;", "sub_82B7A0B0(ctx, base);")
    ins("addi r3,r31,208", "ctx.r3.s64 = ctx.r31.s64 + 208;")
    ins("mr r4,r30", "ctx.r4.u64 = ctx.r30.u64;")
    ins("li r5,64", "ctx.r5.s64 = 64;")
    ins("bl 0x82b7a0b0", "ctx.lr = 0x8268CC88;", "sub_82B7A0B0(ctx, base);")
    expected.append("loc_8268CC88:")
    ins("addi r1,r1,112", "ctx.r1.s64 = ctx.r1.s64 + 112;")
    ins("lwz r12,-8(r1)", "ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);")
    ins("mtlr r12", "ctx.lr = ctx.r12.u64;")
    ins("ld r30,-24(r1)", "ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);")
    ins("ld r31,-16(r1)", "ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);")
    ins("blr", "return;")
    expected.append("}")
    if entry["instructions"] != instructions or body != expected or \
            entry["cfg_branches"] != [[7, "beq", 24]] or \
            entry["direct_calls"] != ["82B7A0B0", "82B7A0B0"]:
        raise ValueError("complete SceneCapture2D body/CFG/calls changed")
    return {"address": ADDRESS, "vtable": signed_word(-32224, -22080, ADDRESS),
            "default_source": signed_word(-31964, -23264, ADDRESS),
            "source": entry["generated_ppc_path"], "source_line": entry["line"],
            "registered_types": entry["types"], "instructions": instructions,
            "original_body": body}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    cache = ROOT / "out/function-inventory"
    candidates = json.loads((cache / "registered-instance-candidates.json")
                            .read_text(encoding="utf-8"))
    bodies = raw_bodies(cache / "registered-instance-originals.cpp.gz")
    by_address = {entry["address"]: entry for entry in candidates["entries"]}
    entry = checked_spec(by_address[ADDRESS], bodies[ADDRESS])
    manifest = {"schema_version": 1, "family": "instance_copy_initializer",
                "entry_count": 1, "copy_target": "82B7A0B0",
                "copy_bytes": 64, "destination_offsets": [144, 208],
                "frame_bytes": 112, "entries": [entry]}
    manifest_path = ROOT / "LostOdysseyRecompSemantics/instance_copy_initializer_families.json"
    source_path = ROOT / "LostOdysseyRecompSemantics/src/instance_copy_initializer_family.cpp"
    source = source_path.read_text(encoding="utf-8")
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("SceneCapture2D parameter markers changed")
    prefix, rest = source.split(BEGIN, 1)
    _, suffix = rest.split(END, 1)
    table = f"constexpr GuestAddress Entry = 0x{ADDRESS.lower()}u;\n" \
        f"constexpr GuestAddress Vtable = {entry['vtable'].lower()}u;\n" \
        f"constexpr GuestAddress DefaultSource = {entry['default_source'].lower()}u;\n"
    source = prefix + BEGIN + table + END + suffix
    manifest_text = json.dumps(manifest, indent=2) + "\n"
    if args.check:
        if manifest_path.read_text(encoding="utf-8") != manifest_text or \
                source_path.read_text(encoding="utf-8") != source:
            raise ValueError("SceneCapture2D source/manifest differ from exact PPC")
    else:
        write_if_changed(manifest_path, manifest_text)
        write_if_changed(source_path, source)
    print("validated one exact SceneCapture2D copy initializer")


if __name__ == "__main__":
    main()
