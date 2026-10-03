"""Admit fixed complete PPC bodies for descriptor lookup and suffix allocation."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_descriptor_lookup_families.json"
SPECS = (
    ("8229D160", "LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp", 31424, 145, 176, 24,
     ["82296d30", "82296d30"], """PPC_FUNC_IMPL(__imp__sub_8229D160) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6d8
	ctx.lr = 0x8229D168;
	__savegprlr_24(ctx, base);
	// stwu r1,-176(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-176);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r30,r4
	ctx.r30.u64 = ctx.r4.u64;
	// std r5,208(r1)
	PPC_STORE_U64(ctx.r1.u32 + 208, ctx.r5.u64);
	// mr r26,r3
	ctx.r26.u64 = ctx.r3.u64;
	// mr r24,r6
	ctx.r24.u64 = ctx.r6.u64;
	// mr r27,r7
	ctx.r27.u64 = ctx.r7.u64;
	// mr r25,r8
	ctx.r25.u64 = ctx.r8.u64;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
	// beq cr6,0x8229d29c
	if (ctx.cr6.eq) goto loc_8229D29C;
	// lwz r27,212(r1)
	ctx.r27.u64 = PPC_LOAD_U32(ctx.r1.u32 + 212);
	// lis r11,-31953
	ctx.r11.s64 = -2094071808;
	// lwz r29,208(r1)
	ctx.r29.u64 = PPC_LOAD_U32(ctx.r1.u32 + 208);
	// addi r11,r11,9576
	ctx.r11.s64 = ctx.r11.s64 + 9576;
	// xor r10,r27,r29
	ctx.r10.u64 = ctx.r27.u64 ^ ctx.r29.u64;
	// xor r10,r10,r30
	ctx.r10.u64 = ctx.r10.u64 ^ ctx.r30.u64;
	// rlwinm r10,r10,2,17,29
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0x7FFC;
	// lwzx r31,r10,r11
	ctx.r31.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r11.u32);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// beq cr6,0x8229d378
	if (ctx.cr6.eq) goto loc_8229D378;
	// lis r11,-32231
	ctx.r11.s64 = -2112290816;
	// addi r28,r11,-10128
	ctx.r28.s64 = ctx.r11.s64 + -10128;
loc_8229D1BC:
	// lwz r11,4(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, -1, ctx.xer);
	// beq cr6,0x8229d1d0
	if (ctx.cr6.eq) goto loc_8229D1D0;
	// addi r11,r31,44
	ctx.r11.s64 = ctx.r31.s64 + 44;
	// b 0x8229d1ec
	goto loc_8229D1EC;
loc_8229D1D0:
	// li r7,1
	ctx.r7.s64 = 1;
	// li r6,1
	ctx.r6.s64 = 1;
	// li r5,0
	ctx.r5.s64 = 0;
	// mr r4,r28
	ctx.r4.u64 = ctx.r28.u64;
	// addi r3,r1,88
	ctx.r3.s64 = ctx.r1.s64 + 88;
	// bl 0x82296d30
	ctx.lr = 0x8229D1E8;
	sub_82296D30(ctx, base);
	// addi r11,r1,88
	ctx.r11.s64 = ctx.r1.s64 + 88;
loc_8229D1EC:
	// ld r11,0(r11)
	ctx.r11.u64 = PPC_LOAD_U64(ctx.r11.u32 + 0);
	// std r11,80(r1)
	PPC_STORE_U64(ctx.r1.u32 + 80, ctx.r11.u64);
	// lwz r11,80(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 80);
	// cmpw cr6,r11,r29
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r29.s32, ctx.xer);
	// bne cr6,0x8229d258
	if (!ctx.cr6.eq) goto loc_8229D258;
	// lwz r11,84(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 84);
	// cmpw cr6,r11,r27
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r27.s32, ctx.xer);
	// bne cr6,0x8229d258
	if (!ctx.cr6.eq) goto loc_8229D258;
	// ld r11,8(r31)
	ctx.r11.u64 = PPC_LOAD_U64(ctx.r31.u32 + 8);
	// and r11,r11,r25
	ctx.r11.u64 = ctx.r11.u64 & ctx.r25.u64;
	// cmpldi cr6,r11,0
	ctx.cr6.compare<uint64_t>(ctx.r11.u64, 0, ctx.xer);
	// bne cr6,0x8229d258
	if (!ctx.cr6.eq) goto loc_8229D258;
	// cmpdi cr6,r25,-1
	ctx.cr6.compare<int64_t>(ctx.r25.s64, -1, ctx.xer);
	// beq cr6,0x8229d258
	if (ctx.cr6.eq) goto loc_8229D258;
	// lwz r11,40(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 40);
	// cmplw cr6,r11,r30
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, ctx.r30.u32, ctx.xer);
	// bne cr6,0x8229d258
	if (!ctx.cr6.eq) goto loc_8229D258;
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r26.u32, 0, ctx.xer);
	// beq cr6,0x8229d290
	if (ctx.cr6.eq) goto loc_8229D290;
	// lwz r11,52(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 52);
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r24.s32, 0, ctx.xer);
	// beq cr6,0x8229d270
	if (ctx.cr6.eq) goto loc_8229D270;
	// subf r11,r11,r26
	ctx.r11.s64 = ctx.r26.s64 - ctx.r11.s64;
	// cntlzw r11,r11
	ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);
	// rlwinm r11,r11,27,31,31
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x8229d290
	if (!ctx.cr6.eq) goto loc_8229D290;
loc_8229D258:
	// lwz r31,20(r31)
	ctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 20);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// bne cr6,0x8229d1bc
	if (!ctx.cr6.eq) goto loc_8229D1BC;
	// li r3,0
	ctx.r3.s64 = 0;
	// addi r1,r1,176
	ctx.r1.s64 = ctx.r1.s64 + 176;
	// b 0x82b7a728
	__restgprlr_24(ctx, base);
	return;
loc_8229D270:
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x8229d258
	if (ctx.cr6.eq) goto loc_8229D258;
loc_8229D278:
	// cmplw cr6,r11,r26
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, ctx.r26.u32, ctx.xer);
	// beq cr6,0x8229d290
	if (ctx.cr6.eq) goto loc_8229D290;
	// lwz r11,60(r11)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 60);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne cr6,0x8229d278
	if (!ctx.cr6.eq) goto loc_8229D278;
	// b 0x8229d258
	goto loc_8229D258;
loc_8229D290:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,176
	ctx.r1.s64 = ctx.r1.s64 + 176;
	// b 0x82b7a728
	__restgprlr_24(ctx, base);
	return;
loc_8229D29C:
	// lwz r28,212(r1)
	ctx.r28.u64 = PPC_LOAD_U32(ctx.r1.u32 + 212);
	// lis r11,-31952
	ctx.r11.s64 = -2094006272;
	// lwz r30,208(r1)
	ctx.r30.u64 = PPC_LOAD_U32(ctx.r1.u32 + 208);
	// addi r11,r11,-23192
	ctx.r11.s64 = ctx.r11.s64 + -23192;
	// xor r10,r28,r30
	ctx.r10.u64 = ctx.r28.u64 ^ ctx.r30.u64;
	// rlwinm r10,r10,2,17,29
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 2) & 0x7FFC;
	// lwzx r31,r10,r11
	ctx.r31.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r11.u32);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// beq cr6,0x8229d378
	if (ctx.cr6.eq) goto loc_8229D378;
	// lis r11,-32231
	ctx.r11.s64 = -2112290816;
	// addi r29,r11,-10128
	ctx.r29.s64 = ctx.r11.s64 + -10128;
loc_8229D2C8:
	// lwz r11,4(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, -1, ctx.xer);
	// beq cr6,0x8229d2dc
	if (ctx.cr6.eq) goto loc_8229D2DC;
	// addi r11,r31,44
	ctx.r11.s64 = ctx.r31.s64 + 44;
	// b 0x8229d2f8
	goto loc_8229D2F8;
loc_8229D2DC:
	// li r7,1
	ctx.r7.s64 = 1;
	// li r6,1
	ctx.r6.s64 = 1;
	// li r5,0
	ctx.r5.s64 = 0;
	// mr r4,r29
	ctx.r4.u64 = ctx.r29.u64;
	// addi r3,r1,88
	ctx.r3.s64 = ctx.r1.s64 + 88;
	// bl 0x82296d30
	ctx.lr = 0x8229D2F4;
	sub_82296D30(ctx, base);
	// addi r11,r1,88
	ctx.r11.s64 = ctx.r1.s64 + 88;
loc_8229D2F8:
	// ld r11,0(r11)
	ctx.r11.u64 = PPC_LOAD_U64(ctx.r11.u32 + 0);
	// std r11,80(r1)
	PPC_STORE_U64(ctx.r1.u32 + 80, ctx.r11.u64);
	// lwz r11,80(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 80);
	// cmpw cr6,r11,r30
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r30.s32, ctx.xer);
	// bne cr6,0x8229d36c
	if (!ctx.cr6.eq) goto loc_8229D36C;
	// lwz r11,84(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 84);
	// cmpw cr6,r11,r28
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r28.s32, ctx.xer);
	// bne cr6,0x8229d36c
	if (!ctx.cr6.eq) goto loc_8229D36C;
	// ld r11,8(r31)
	ctx.r11.u64 = PPC_LOAD_U64(ctx.r31.u32 + 8);
	// and r11,r11,r25
	ctx.r11.u64 = ctx.r11.u64 & ctx.r25.u64;
	// cmpldi cr6,r11,0
	ctx.cr6.compare<uint64_t>(ctx.r11.u64, 0, ctx.xer);
	// bne cr6,0x8229d36c
	if (!ctx.cr6.eq) goto loc_8229D36C;
	// cmpdi cr6,r25,-1
	ctx.cr6.compare<int64_t>(ctx.r25.s64, -1, ctx.xer);
	// beq cr6,0x8229d36c
	if (ctx.cr6.eq) goto loc_8229D36C;
	// cmpwi cr6,r27,0
	ctx.cr6.compare<int32_t>(ctx.r27.s32, 0, ctx.xer);
	// bne cr6,0x8229d344
	if (!ctx.cr6.eq) goto loc_8229D344;
	// lwz r11,40(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 40);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne cr6,0x8229d36c
	if (!ctx.cr6.eq) goto loc_8229D36C;
loc_8229D344:
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r26.u32, 0, ctx.xer);
	// beq cr6,0x8229d290
	if (ctx.cr6.eq) goto loc_8229D290;
	// lwz r11,52(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 52);
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r24.s32, 0, ctx.xer);
	// beq cr6,0x8229d384
	if (ctx.cr6.eq) goto loc_8229D384;
	// subf r11,r11,r26
	ctx.r11.s64 = ctx.r26.s64 - ctx.r11.s64;
	// cntlzw r11,r11
	ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);
	// rlwinm r11,r11,27,31,31
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x8229d290
	if (!ctx.cr6.eq) goto loc_8229D290;
loc_8229D36C:
	// lwz r31,16(r31)
	ctx.r31.u64 = PPC_LOAD_U32(ctx.r31.u32 + 16);
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, 0, ctx.xer);
	// bne cr6,0x8229d2c8
	if (!ctx.cr6.eq) goto loc_8229D2C8;
loc_8229D378:
	// li r3,0
	ctx.r3.s64 = 0;
	// addi r1,r1,176
	ctx.r1.s64 = ctx.r1.s64 + 176;
	// b 0x82b7a728
	__restgprlr_24(ctx, base);
	return;
loc_8229D384:
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x8229d36c
	if (ctx.cr6.eq) goto loc_8229D36C;
loc_8229D38C:
	// cmplw cr6,r11,r26
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, ctx.r26.u32, ctx.xer);
	// beq cr6,0x8229d290
	if (ctx.cr6.eq) goto loc_8229D290;
	// lwz r11,60(r11)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 60);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// bne cr6,0x8229d38c
	if (!ctx.cr6.eq) goto loc_8229D38C;
	// b 0x8229d36c
	goto loc_8229D36C;
}"""),
    ("82400A30", "LostOdysseyRecompLib/ppc/ppc_recomp.15.cpp", 31175, 50, 144, 27,
     ["82296d30", "8229d160"], """PPC_FUNC_IMPL(__imp__sub_82400A30) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x82400A38;
	__savegprlr_27(ctx, base);
	// stwu r1,-144(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// std r6,184(r1)
	PPC_STORE_U64(ctx.r1.u32 + 184, ctx.r6.u64);
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// mr r27,r4
	ctx.r27.u64 = ctx.r4.u64;
	// mr r30,r5
	ctx.r30.u64 = ctx.r5.u64;
	// lwz r29,184(r1)
	ctx.r29.u64 = PPC_LOAD_U32(ctx.r1.u32 + 184);
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r29.s32, 0, ctx.xer);
	// bne cr6,0x82400aa4
	if (!ctx.cr6.eq) goto loc_82400AA4;
	// lwz r11,188(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 188);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x82400aa4
	if (!ctx.cr6.eq) goto loc_82400AA4;
	// lwz r11,4(r30)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r30.u32 + 4);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r11.s32, -1, ctx.xer);
	// beq cr6,0x82400a78
	if (ctx.cr6.eq) goto loc_82400A78;
	// addi r11,r30,44
	ctx.r11.s64 = ctx.r30.s64 + 44;
	// b 0x82400a98
	goto loc_82400A98;
loc_82400A78:
	// lis r11,-32231
	ctx.r11.s64 = -2112290816;
	// li r7,1
	ctx.r7.s64 = 1;
	// addi r4,r11,-10128
	ctx.r4.s64 = ctx.r11.s64 + -10128;
	// li r6,1
	ctx.r6.s64 = 1;
	// li r5,0
	ctx.r5.s64 = 0;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x82296d30
	ctx.lr = 0x82400A94;
	sub_82296D30(ctx, base);
	// addi r11,r1,80
	ctx.r11.s64 = ctx.r1.s64 + 80;
loc_82400A98:
	// ld r11,0(r11)
	ctx.r11.u64 = PPC_LOAD_U64(ctx.r11.u32 + 0);
	// std r11,80(r1)
	PPC_STORE_U64(ctx.r1.u32 + 80, ctx.r11.u64);
	// lwz r29,80(r1)
	ctx.r29.u64 = PPC_LOAD_U32(ctx.r1.u32 + 80);
loc_82400AA4:
	// li r11,-1
	ctx.r11.s64 = -1;
	// subf r11,r27,r11
	ctx.r11.s64 = ctx.r11.s64 - ctx.r27.s64;
	// cntlzw r11,r11
	ctx.r11.u64 = ctx.r11.u32 == 0 ? 32 : __builtin_clz(ctx.r11.u32);
	// rlwinm r28,r11,27,31,31
	ctx.r28.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 27) & 0x1;
loc_82400AB4:
	// lwz r11,188(r30)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r30.u32 + 188);
	// li r8,0
	ctx.r8.s64 = 0;
	// mr r7,r28
	ctx.r7.u64 = ctx.r28.u64;
	// stw r29,0(r31)
	PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r29.u32);
	// addi r11,r11,1
	ctx.r11.s64 = ctx.r11.s64 + 1;
	// li r6,0
	ctx.r6.s64 = 0;
	// mr r4,r27
	ctx.r4.u64 = ctx.r27.u64;
	// li r3,0
	ctx.r3.s64 = 0;
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r31.u32 + 4, ctx.r11.u32);
	// ld r5,0(r31)
	ctx.r5.u64 = PPC_LOAD_U64(ctx.r31.u32 + 0);
	// stw r11,188(r30)
	PPC_STORE_U32(ctx.r30.u32 + 188, ctx.r11.u32);
	// bl 0x8229d160
	ctx.lr = 0x82400AE4;
	sub_8229D160(ctx, base);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// bne cr6,0x82400ab4
	if (!ctx.cr6.eq) goto loc_82400AB4;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,144
	ctx.r1.s64 = ctx.r1.s64 + 144;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
}"""),
)


def generate() -> dict:
    entries = []
    for address, relative, line, count, frame, first, expected_calls, expected in SPECS:
        source = ROOT / relative
        if not source.exists():
            source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / relative
        lines = source.read_text(encoding="utf-8").splitlines()
        if lines[line - 1] != f"PPC_FUNC_IMPL(__imp__sub_{address}) {{":
            raise ValueError(f"{address} moved from fixed PPC source line")
        end = line
        while end < len(lines) and lines[end] != "}":
            end += 1
        if end == len(lines):
            raise ValueError(f"{address} body is unterminated")
        observed = "\n".join(lines[line - 1:end + 1])
        if observed != expected.strip("\n"):
            raise ValueError(f"{address} full translated body differs")
        instructions = re.findall(r"^[ \t]*// (.+)$", observed, re.MULTILINE)
        if len(instructions) != count:
            raise ValueError(f"{address} instruction count differs")
        calls = [call.lower() for call in re.findall(r"// bl 0x([0-9a-f]+)", observed)]
        if calls[1:] != expected_calls:
            raise ValueError(f"{address} direct-call sequence differs: {calls}")
        labels = re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE)
        branches = re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE)
        entries.append({
            "address": address, "source": relative, "source_line": line,
            "frame_size": frame, "save_first": first,
            "instructions": instructions, "calls": [c.upper() for c in expected_calls],
            "cfg": {"labels": labels, "branches": branches},
            "translated_body": observed, "status": "bounded_readable_library",
            "boundary": "real recovered NameLookup; its external service/volatile ABI limits carry through",
        })
    return {"schema": "metadata-descriptor-lookup-v1", "entries": entries}


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
