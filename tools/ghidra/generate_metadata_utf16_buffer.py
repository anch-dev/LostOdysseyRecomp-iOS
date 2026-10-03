"""Admit both exact 8232D378/8232D418 UTF-16 buffer bodies."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_utf16_buffer_families.json"
SOURCE = Path("LostOdysseyRecompLib/ppc/ppc_recomp.6.cpp")
SPECS = (("8232D378", 15255, 39, "append_utf16"),
         ("8232D418", 15355, 33, "compose_utf16"))
EXPECTED = (
    """PPC_FUNC_IMPL(__imp__sub_8232D378) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x8232D380;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r30,r4
	ctx.r30.u64 = ctx.r4.u64;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// lhz r11,0(r30)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r30.u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x8232d408
	if (ctx.cr6.eq) goto loc_8232D408;
	// lwz r11,4(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq cr6,0x8232d3e4
	if (ctx.cr6.eq) goto loc_8232D3E4;
	// addi r29,r11,-1
	ctx.r29.s64 = ctx.r11.s64 + -1;
	// bl 0x82296830
	ctx.lr = 0x8232D3B0;
	sub_82296830(ctx, base);
	// mr r4,r3
	ctx.r4.u64 = ctx.r3.u64;
	// li r6,8
	ctx.r6.s64 = 8;
	// li r5,2
	ctx.r5.s64 = 2;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822c42d8
	ctx.lr = 0x8232D3C4;
	sub_822C42D8(ctx, base);
	// lwz r10,0(r31)
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	// rlwinm r11,r29,1,0,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r29.u32 | (ctx.r29.u64 << 32), 1) & 0xFFFFFFFE;
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// add r3,r11,r10
	ctx.r3.u64 = ctx.r11.u64 + ctx.r10.u64;
	// bl 0x8230bac0
	ctx.lr = 0x8232D3D8;
	sub_8230BAC0(ctx, base);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
loc_8232D3E4:
	// bl 0x82296830
	ctx.lr = 0x8232D3E8;
	sub_82296830(ctx, base);
	// addi r4,r3,1
	ctx.r4.s64 = ctx.r3.s64 + 1;
	// li r6,8
	ctx.r6.s64 = 8;
	// li r5,2
	ctx.r5.s64 = 2;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822c42d8
	ctx.lr = 0x8232D3FC;
	sub_822C42D8(ctx, base);
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// lwz r3,0(r31)
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	// bl 0x8230bac0
	ctx.lr = 0x8232D408;
	sub_8230BAC0(ctx, base);
loc_8232D408:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}""",
    """PPC_FUNC_IMPL(__imp__sub_8232D418) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
	// std r30,-24(r1)
	PPC_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// mr r30,r5
	ctx.r30.u64 = ctx.r5.u64;
	// bl 0x822a06c0
	ctx.lr = 0x8232D43C;
	sub_822A06C0(ctx, base);
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// bl 0x8232d378
	ctx.lr = 0x8232D444;
	sub_8232D378(ctx, base);
	// mr r4,r3
	ctx.r4.u64 = ctx.r3.u64;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822a06c0
	ctx.lr = 0x8232D450;
	sub_822A06C0(ctx, base);
	// li r11,0
	ctx.r11.s64 = 0;
	// lwz r10,88(r1)
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r1.u32 + 88);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// stw r11,84(r1)
	PPC_STORE_U32(ctx.r1.u32 + 84, ctx.r11.u32);
	// beq cr6,0x8232d478
	if (ctx.cr6.eq) goto loc_8232D478;
	// li r5,8
	ctx.r5.s64 = 8;
	// stw r11,88(r1)
	PPC_STORE_U32(ctx.r1.u32 + 88, ctx.r11.u32);
	// li r4,2
	ctx.r4.s64 = 2;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x8229f678
	ctx.lr = 0x8232D478;
	sub_8229F678(ctx, base);
loc_8232D478:
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x82298a98
	ctx.lr = 0x8232D480;
	sub_82298A98(ctx, base);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,128
	ctx.r1.s64 = ctx.r1.s64 + 128;
	// lwz r12,-8(r1)
	ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r30,-24(r1)
	ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 + -24);
	// ld r31,-16(r1)
	ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
	// blr\x20
	return;
}""",

)
CALLS = {
    "8232D378": ["82296830", "822c42d8", "8230bac0", "82296830",
                 "822c42d8", "8230bac0"],
    "8232D418": ["822a06c0", "8232d378", "822a06c0", "8229f678",
                 "82298a98"],
}
LABELS = {
    "8232D378": ["8232D3E4", "8232D408"],
    "8232D418": ["8232D478"],
}


def generate() -> dict:
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    entries = []
    for (address, source_line, instruction_count, shape), expected in zip(SPECS, EXPECTED):
        if lines[source_line - 1] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"{address} moved from fixed PPC source line")
        end = source_line
        while lines[end] != "}":
            end += 1
        observed = "\n".join(lines[source_line - 1:end + 1])
        if observed != expected.rstrip("\n"):
            raise ValueError(f"{address} full translated body differs")
        instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
        if len(instructions) != instruction_count:
            raise ValueError(f"{address} instruction count differs")
        calls = [x.lower() for x in re.findall(r"// bl 0x([0-9a-f]+)", observed)
                 if x.lower() not in ("82b7a6ec",)]
        if calls != CALLS[address]:
            raise ValueError(f"{address} direct-call sequence differs")
        labels = re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE)
        if labels != LABELS[address]:
            raise ValueError(f"{address} CFG labels differ")
        branches = re.findall(r"^[ \t]*// (b(?:ne|eq) cr6,0x[0-9a-f]+)$",
                              observed, re.MULTILINE)
        entries.append({
            "address": address, "source": str(SOURCE).replace("\\", "/"),
            "source_line": source_line, "shape": shape,
            "frame_size": 112 if address == "8232D378" else 128,
            "save_first": 29 if address == "8232D378" else 30,
            "instructions": instructions, "calls": [c.upper() for c in calls],
            "cfg": {"labels": labels, "branches": branches},
            "translated_body": observed, "status": "bounded_readable_library",
            "boundary": "existing UTF16/array/copy/release lower models; dynamic resize/release and generic lower ABI explicit",
        })
    return {"schema": "metadata-utf16-buffer-v1", "entries": entries}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=MANIFEST)
    args = parser.parse_args()
    payload = json.dumps(generate(), indent=2) + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding="utf-8") != payload:
        args.output.write_text(payload, encoding="utf-8")


if __name__ == "__main__":
    main()
