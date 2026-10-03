"""Admit the exact 82296D30 metadata-name lookup body and its CFG."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_name_lookup_families.json"
SOURCE = Path("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp")
SOURCE_LINE = 16272
EXPECTED_BODY = """PPC_FUNC_IMPL(__imp__sub_82296D30) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x82296D38;
	__savegprlr_26(ctx, base);
	// stwu r1,-416(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-416);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// lis r11,-31964
	ctx.r11.s64 = -2094792704;
	// mr r30,r3
	ctx.r30.u64 = ctx.r3.u64;
	// mr r28,r4
	ctx.r28.u64 = ctx.r4.u64;
	// mr r31,r5
	ctx.r31.u64 = ctx.r5.u64;
	// mr r26,r6
	ctx.r26.u64 = ctx.r6.u64;
	// lwz r11,25184(r11)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 25184);
	// mr r29,r7
	ctx.r29.u64 = ctx.r7.u64;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x82296d64
	if (!ctx.cr6.eq) goto loc_82296D64;
	// bl 0x823f4700
	ctx.lr = 0x82296D64;
	sub_823F4700(ctx, base);
loc_82296D64:
	// cmpwi cr6,r31,0
	ctx.cr6.compare<int32_t>(ctx.r31.s32, 0, ctx.xer);
	// bne cr6,0x82296d9c
	if (!ctx.cr6.eq) goto loc_82296D9C;
	// cmpwi cr6,r29,1
	ctx.cr6.compare<int32_t>(ctx.r29.s32, 1, ctx.xer);
	// bne cr6,0x82296d9c
	if (!ctx.cr6.eq) goto loc_82296D9C;
	// addi r6,r1,80
	ctx.r6.s64 = ctx.r1.s64 + 80;
	// li r5,128
	ctx.r5.s64 = 128;
	// addi r4,r1,96
	ctx.r4.s64 = ctx.r1.s64 + 96;
	// mr r3,r28
	ctx.r3.u64 = ctx.r28.u64;
	// bl 0x82296e80
	ctx.lr = 0x82296D88;
	sub_82296E80(ctx, base);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// beq cr6,0x82296d9c
	if (ctx.cr6.eq) goto loc_82296D9C;
	// lwz r11,80(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 80);
	// addi r28,r1,96
	ctx.r28.s64 = ctx.r1.s64 + 96;
	// addi r31,r11,1
	ctx.r31.s64 = ctx.r11.s64 + 1;
loc_82296D9C:
	// lhz r11,0(r28)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r28.u32 + 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne cr6,0x82296dbc
	if (!ctx.cr6.eq) goto loc_82296DBC;
loc_82296DA8:
	// li r11,0
	ctx.r11.s64 = 0;
	// stw r11,0(r30)
	PPC_STORE_U32(ctx.r30.u32 + 0, ctx.r11.u32);
	// stw r11,4(r30)
	PPC_STORE_U32(ctx.r30.u32 + 4, ctx.r11.u32);
	// addi r1,r1,416
	ctx.r1.s64 = ctx.r1.s64 + 416;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
loc_82296DBC:
	// mr r3,r28
	ctx.r3.u64 = ctx.r28.u64;
	// stw r31,4(r30)
	PPC_STORE_U32(ctx.r30.u32 + 4, ctx.r31.u32);
	// bl 0x82296f68
	ctx.lr = 0x82296DC8;
	sub_82296F68(ctx, base);
	// lis r10,-31953
	ctx.r10.s64 = -2094071808;
	// rlwinm r27,r3,2,18,29
	ctx.r27.u64 = __builtin_rotateleft64(ctx.r3.u32 | (ctx.r3.u64 << 32), 2) & 0x3FFC;
	// addi r29,r10,-6808
	ctx.r29.s64 = ctx.r10.s64 + -6808;
	// lwzx r31,r27,r29
	ctx.r31.u64 = PPC_LOAD_U32(ctx.r27.u32 + ctx.r29.u32);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// beq cr6,0x82296e00
	if (ctx.cr6.eq) goto loc_82296E00;
loc_82296DE0:
	// addi r4,r31,16
	ctx.r4.s64 = ctx.r31.s64 + 16;
	// mr r3,r28
	ctx.r3.u64 = ctx.r28.u64;
	// bl 0x822971e0
	ctx.lr = 0x82296DEC;
	sub_822971E0(ctx, base);
	// cmpwi cr6,r3,0
	ctx.cr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// beq cr6,0x82296e58
	if (ctx.cr6.eq) goto loc_82296E58;
	// lwz r31,12(r31)
	ctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 12);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// bne cr6,0x82296de0
	if (!ctx.cr6.eq) goto loc_82296DE0;
loc_82296E00:
	// cmpwi cr6,r26,0
	ctx.cr6.compare<int32_t>(ctx.r26.s32, 0, ctx.xer);
	// beq cr6,0x82296da8
	if (ctx.cr6.eq) goto loc_82296DA8;
	// lis r11,-31945
	ctx.r11.s64 = -2093547520;
	// li r6,8
	ctx.r6.s64 = 8;
	// addi r31,r11,-28464
	ctx.r31.s64 = ctx.r11.s64 + -28464;
	// li r5,4
	ctx.r5.s64 = 4;
	// li r4,1
	ctx.r4.s64 = 1;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822c42d8
	ctx.lr = 0x82296E24;
	sub_822C42D8(ctx, base);
	// mr r4,r3
	ctx.r4.u64 = ctx.r3.u64;
	// li r5,0
	ctx.r5.s64 = 0;
	// mr r3,r28
	ctx.r3.u64 = ctx.r28.u64;
	// stw r4,0(r30)
	PPC_STORE_U32(ctx.r30.u32 + 0, ctx.r4.u32);
	// lwzx r6,r27,r29
	ctx.r6.u64 = PPC_LOAD_U32(ctx.r27.u32 + ctx.r29.u32);
	// bl 0x823f7b08
	ctx.lr = 0x82296E3C;
	sub_823F7B08(ctx, base);
	// stwx r3,r27,r29
	PPC_STORE_U32(ctx.r27.u32 + ctx.r29.u32, ctx.r3.u32);
	// lwz r11,0(r30)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r30.u32 + 0);
	// rlwinm r10,r11,2,0,29
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
	// lwz r11,0(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	// stwx r3,r10,r11
	PPC_STORE_U32(ctx.r10.u32 + ctx.r11.u32, ctx.r3.u32);
loc_82296E50:
	// addi r1,r1,416
	ctx.r1.s64 = ctx.r1.s64 + 416;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
loc_82296E58:
	// lwz r11,0(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	// cmpwi cr6,r26,2
	ctx.cr6.compare<int32_t>(ctx.r26.s32, 2, ctx.xer);
	// stw r11,0(r30)
	PPC_STORE_U32(ctx.r30.u32 + 0, ctx.r11.u32);
	// bne cr6,0x82296e50
	if (!ctx.cr6.eq) goto loc_82296E50;
	// mr r4,r28
	ctx.r4.u64 = ctx.r28.u64;
	// addi r3,r31,16
	ctx.r3.s64 = ctx.r31.s64 + 16;
	// bl 0x8230bac0
	ctx.lr = 0x82296E74;
	sub_8230BAC0(ctx, base);
	// addi r1,r1,416
	ctx.r1.s64 = ctx.r1.s64 + 416;
	// b 0x82b7a730
	__restgprlr_26(ctx, base);
	return;
}
"""
EXPECTED_CALLS = ["823f4700", "82296e80", "82296f68", "822971e0",
                  "822c42d8", "823f7b08", "8230bac0"]
EXPECTED_LABELS = ["82296D64", "82296D9C", "82296DA8", "82296DBC",
                   "82296DE0", "82296E00", "82296E50", "82296E58"]


def generate() -> dict:
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    if lines[SOURCE_LINE - 1] != "PPC_FUNC_IMPL(__imp__sub_82296D30) {":
        raise ValueError("82296D30 moved from fixed PPC source line")
    end = SOURCE_LINE
    while lines[end] != "}":
        end += 1
    observed = "\n".join(lines[SOURCE_LINE - 1:end + 1])
    if observed != EXPECTED_BODY.rstrip("\n"):
        raise ValueError("82296D30 full translated body differs")
    instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
    if len(instructions) != 83:
        raise ValueError("82296D30 instruction count differs")
    direct_calls = re.findall(r"// bl 0x([0-9a-f]+)", observed)
    if [x.lower() for x in direct_calls if x.lower() != "82b7a6e0"] != EXPECTED_CALLS:
        raise ValueError("82296D30 direct-call sequence differs")
    labels = re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE)
    if labels != EXPECTED_LABELS:
        raise ValueError("82296D30 CFG labels differ")
    branches = re.findall(r"^[ \t]*// (b(?:ne|eq) cr6,0x[0-9a-f]+)$", observed, re.MULTILINE)
    if len(branches) != 10:
        raise ValueError("82296D30 CFG branches differ")
    return {"schema": "metadata-name-lookup-v1", "entries": [{
        "address": "82296D30", "source": str(SOURCE).replace("\\", "/"),
        "source_line": SOURCE_LINE, "frame_size": 416, "save_first": 26,
        "instructions": instructions, "calls": [x.upper() for x in EXPECTED_CALLS],
        "cfg": {"labels": labels, "branches": branches},
        "globals": {"ready": "83246260", "buckets": "832EE568",
                    "id_index": "833690D0"},
        "translated_body": observed, "status": "bounded_readable_library",
        "boundary": "dynamic record allocator and resize/CRT services explicit; generic lower volatile ABI excluded",
    }]}


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
