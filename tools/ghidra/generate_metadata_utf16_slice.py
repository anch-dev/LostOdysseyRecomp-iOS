"""Admit four exact PPC UTF-16 slice/reverse/format bodies at fixed source lines."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path("LostOdysseyRecompLib/ppc/ppc_recomp.6.cpp")
MANIFEST = ROOT / "LostOdysseyRecompSemantics/metadata_utf16_slice_families.json"
SPECS = (
    ("8232D240", 15060, 33, 128, 27,
     ["8229f678", "8232d318"], """PPC_FUNC_IMPL(__imp__sub_8232D240) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x8232D248;
	__savegprlr_27(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r29,r4
	ctx.r29.u64 = ctx.r4.u64;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// mr r27,r5
	ctx.r27.u64 = ctx.r5.u64;
	// li r28,0
	ctx.r28.s64 = 0;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r29.s32, 0, ctx.xer);
	// addi r11,r29,1
	ctx.r11.s64 = ctx.r29.s64 + 1;
	// bne cr6,0x8232d26c
	if (!ctx.cr6.eq) goto loc_8232D26C;
	// mr r11,r28
	ctx.r11.u64 = ctx.r28.u64;
loc_8232D26C:
	// li r5,8
	ctx.r5.s64 = 8;
	// stw r28,0(r31)
	PPC_STORE_U32(ctx.r31.u32 + 0, ctx.r28.u32);
	// li r4,2
	ctx.r4.s64 = 2;
	// stw r11,4(r31)
	PPC_STORE_U32(ctx.r31.u32 + 4, ctx.r11.u32);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// stw r11,8(r31)
	PPC_STORE_U32(ctx.r31.u32 + 8, ctx.r11.u32);
	// bl 0x8229f678
	ctx.lr = 0x8232D288;
	sub_8229F678(ctx, base);
	// lwz r11,4(r31)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r31.u32 + 4);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq cr6,0x8232d2b8
	if (ctx.cr6.eq) goto loc_8232D2B8;
	// lwz r30,0(r31)
	ctx.r30.u64 = PPC_LOAD_U32(ctx.r31.u32 + 0);
	// addi r29,r29,1
	ctx.r29.s64 = ctx.r29.s64 + 1;
	// mr r4,r27
	ctx.r4.u64 = ctx.r27.u64;
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// mr r5,r29
	ctx.r5.u64 = ctx.r29.u64;
	// bl 0x8232d318
	ctx.lr = 0x8232D2AC;
	sub_8232D318(ctx, base);
	// rlwinm r11,r29,1,0,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r29.u32 | (ctx.r29.u64 << 32), 1) & 0xFFFFFFFE;
	// add r11,r11,r30
	ctx.r11.u64 = ctx.r11.u64 + ctx.r30.u64;
	// sth r28,-2(r11)
	PPC_STORE_U16(ctx.r11.u32 + -2, ctx.r28.u16);
loc_8232D2B8:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,128
	ctx.r1.s64 = ctx.r1.s64 + 128;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
}"""),
    ("8232D190", 14954, 43, 96, 31,
     ["8232d240"], """PPC_FUNC_IMPL(__imp__sub_8232D190) {
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
	// lwz r10,4(r4)
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r4.u32 + 4);
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// add r9,r5,r6
	ctx.r9.u64 = ctx.r5.u64 + ctx.r6.u64;
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq cr6,0x8232d1e4
	if (ctx.cr6.eq) goto loc_8232D1E4;
	// addi r11,r10,-1
	ctx.r11.s64 = ctx.r10.s64 + -1;
	// cmplw cr6,r5,r11
	ctx.cr6.compare<uint32_t>(ctx.r5.u32, ctx.r11.u32, ctx.xer);
	// blt cr6,0x8232d1c4
	if (ctx.cr6.lt) goto loc_8232D1C4;
loc_8232D1C0:
	// mr r5,r11
	ctx.r5.u64 = ctx.r11.u64;
loc_8232D1C4:
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// addi r11,r10,-1
	ctx.r11.s64 = ctx.r10.s64 + -1;
	// bne cr6,0x8232d1d4
	if (!ctx.cr6.eq) goto loc_8232D1D4;
	// li r11,0
	ctx.r11.s64 = 0;
loc_8232D1D4:
	// cmplw cr6,r9,r5
	ctx.cr6.compare<uint32_t>(ctx.r9.u32, ctx.r5.u32, ctx.xer);
	// bge cr6,0x8232d1ec
	if (!ctx.cr6.lt) goto loc_8232D1EC;
	// mr r9,r5
	ctx.r9.u64 = ctx.r5.u64;
	// b 0x8232d1f8
	goto loc_8232D1F8;
loc_8232D1E4:
	// li r11,0
	ctx.r11.s64 = 0;
	// b 0x8232d1c0
	goto loc_8232D1C0;
loc_8232D1EC:
	// cmplw cr6,r9,r11
	ctx.cr6.compare<uint32_t>(ctx.r9.u32, ctx.r11.u32, ctx.xer);
	// blt cr6,0x8232d1f8
	if (ctx.cr6.lt) goto loc_8232D1F8;
	// mr r9,r11
	ctx.r9.u64 = ctx.r11.u64;
loc_8232D1F8:
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq cr6,0x8232d208
	if (ctx.cr6.eq) goto loc_8232D208;
	// lwz r10,0(r4)
	ctx.r10.u64 = PPC_LOAD_U32(ctx.r4.u32 + 0);
	// b 0x8232d210
	goto loc_8232D210;
loc_8232D208:
	// lis r11,-32229
	ctx.r11.s64 = -2112159744;
	// addi r10,r11,-31792
	ctx.r10.s64 = ctx.r11.s64 + -31792;
loc_8232D210:
	// rlwinm r11,r5,1,0,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r5.u32 | (ctx.r5.u64 << 32), 1) & 0xFFFFFFFE;
	// subf r4,r5,r9
	ctx.r4.s64 = ctx.r9.s64 - ctx.r5.s64;
	// add r5,r11,r10
	ctx.r5.u64 = ctx.r11.u64 + ctx.r10.u64;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x8232d240
	ctx.lr = 0x8232D224;
	sub_8232D240(ctx, base);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// addi r1,r1,96
	ctx.r1.s64 = ctx.r1.s64 + 96;
	// lwz r12,-8(r1)
	ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r12.u64;
	// ld r31,-16(r1)
	ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 + -16);
	// blr\x20
	return;
}"""),
    ("8232D040", 14757, 84, 160, 25,
     ["8232d190", "8232d378", "827c5f38", "82298af8", "827c5f38"],
     """PPC_FUNC_IMPL(__imp__sub_8232D040) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x8232D048;
	__savegprlr_25(ctx, base);
	// stwu r1,-160(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r27,r3
	ctx.r27.u64 = ctx.r3.u64;
	// li r29,0
	ctx.r29.s64 = 0;
	// mr r26,r4
	ctx.r26.u64 = ctx.r4.u64;
	// stw r29,0(r27)
	PPC_STORE_U32(ctx.r27.u32 + 0, ctx.r29.u32);
	// stw r29,4(r27)
	PPC_STORE_U32(ctx.r27.u32 + 4, ctx.r29.u32);
	// stw r29,8(r27)
	PPC_STORE_U32(ctx.r27.u32 + 8, ctx.r29.u32);
	// lwz r11,4(r26)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r26.u32 + 4);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// addi r11,r11,-1
	ctx.r11.s64 = ctx.r11.s64 + -1;
	// bne cr6,0x8232d078
	if (!ctx.cr6.eq) goto loc_8232D078;
	// mr r11,r29
	ctx.r11.u64 = ctx.r29.u64;
loc_8232D078:
	// addi r28,r11,-1
	ctx.r28.s64 = ctx.r11.s64 + -1;
	// cmpwi cr6,r28,-1
	ctx.cr6.compare<int32_t>(ctx.r28.s32, -1, ctx.xer);
	// ble cr6,0x8232d184
	if (!ctx.cr6.gt) goto loc_8232D184;
	// lis r11,-32229
	ctx.r11.s64 = -2112159744;
	// lis r30,-31951
	ctx.r30.s64 = -2093940736;
	// addi r25,r11,-31792
	ctx.r25.s64 = ctx.r11.s64 + -31792;
loc_8232D090:
	// li r6,1
	ctx.r6.s64 = 1;
	// mr r5,r28
	ctx.r5.u64 = ctx.r28.u64;
	// mr r4,r26
	ctx.r4.u64 = ctx.r26.u64;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x8232d190
	ctx.lr = 0x8232D0A4;
	sub_8232D190(ctx, base);
	// lwz r11,4(r3)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 4);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq cr6,0x8232d0b8
	if (ctx.cr6.eq) goto loc_8232D0B8;
	// lwz r4,0(r3)
	ctx.r4.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
	// b 0x8232d0bc
	goto loc_8232D0BC;
loc_8232D0B8:
	// mr r4,r25
	ctx.r4.u64 = ctx.r25.u64;
loc_8232D0BC:
	// mr r3,r27
	ctx.r3.u64 = ctx.r27.u64;
	// bl 0x8232d378
	ctx.lr = 0x8232D0C4;
	sub_8232D378(ctx, base);
	// lwz r11,88(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 88);
	// stw r29,84(r1)
	PPC_STORE_U32(ctx.r1.u32 + 84, ctx.r29.u32);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq cr6,0x8232d11c
	if (ctx.cr6.eq) goto loc_8232D11C;
	// lwz r11,80(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 80);
	// stw r29,88(r1)
	PPC_STORE_U32(ctx.r1.u32 + 88, ctx.r29.u32);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x8232d11c
	if (ctx.cr6.eq) goto loc_8232D11C;
	// lwz r3,-18936(r30)
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + -18936);
	// mr r31,r11
	ctx.r31.u64 = ctx.r11.u64;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// bne cr6,0x8232d0fc
	if (!ctx.cr6.eq) goto loc_8232D0FC;
	// bl 0x827c5f38
	ctx.lr = 0x8232D0F8;
	sub_827C5F38(ctx, base);
	// lwz r3,-18936(r30)
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + -18936);
loc_8232D0FC:
	// lwz r11,0(r3)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
	// li r6,8
	ctx.r6.s64 = 8;
	// li r5,0
	ctx.r5.s64 = 0;
	// mr r4,r31
	ctx.r4.u64 = ctx.r31.u64;
	// lwz r11,8(r11)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 8);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl\x20
	ctx.lr = 0x8232D118;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
	// stw r3,80(r1)
	PPC_STORE_U32(ctx.r1.u32 + 80, ctx.r3.u32);
loc_8232D11C:
	// li r7,8
	ctx.r7.s64 = 8;
	// lwz r5,84(r1)
	ctx.r5.u64 = PPC_LOAD_U32(ctx.r1.u32 + 84);
	// li r6,2
	ctx.r6.s64 = 2;
	// li r4,0
	ctx.r4.s64 = 0;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x82298af8
	ctx.lr = 0x8232D134;
	sub_82298AF8(ctx, base);
	// lwz r11,80(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 80);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x8232d16c
	if (ctx.cr6.eq) goto loc_8232D16C;
	// lwz r3,-18936(r30)
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + -18936);
	// mr r31,r11
	ctx.r31.u64 = ctx.r11.u64;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// bne cr6,0x8232d158
	if (!ctx.cr6.eq) goto loc_8232D158;
	// bl 0x827c5f38
	ctx.lr = 0x8232D154;
	sub_827C5F38(ctx, base);
	// lwz r3,-18936(r30)
	ctx.r3.u64 = PPC_LOAD_U32(ctx.r30.u32 + -18936);
loc_8232D158:
	// lwz r11,0(r3)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
	// mr r4,r31
	ctx.r4.u64 = ctx.r31.u64;
	// lwz r11,12(r11)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 12);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctrl\x20
	ctx.lr = 0x8232D16C;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
loc_8232D16C:
	// addi r28,r28,-1
	ctx.r28.s64 = ctx.r28.s64 + -1;
	// stw r29,80(r1)
	PPC_STORE_U32(ctx.r1.u32 + 80, ctx.r29.u32);
	// stw r29,88(r1)
	PPC_STORE_U32(ctx.r1.u32 + 88, ctx.r29.u32);
	// stw r29,84(r1)
	PPC_STORE_U32(ctx.r1.u32 + 84, ctx.r29.u32);
	// cmpwi cr6,r28,-1
	ctx.cr6.compare<int32_t>(ctx.r28.s32, -1, ctx.xer);
	// bgt cr6,0x8232d090
	if (ctx.cr6.gt) goto loc_8232D090;
loc_8232D184:
	// mr r3,r27
	ctx.r3.u64 = ctx.r27.u64;
	// addi r1,r1,160
	ctx.r1.s64 = ctx.r1.s64 + 160;
	// b 0x82b7a72c
	__restgprlr_25(ctx, base);
	return;
}"""),
    ("8232CED8", 14555, 90, 176, 28,
     ["8232d378", "8232d378", "8232d040", "8229f678", "82298a98"],
     """PPC_FUNC_IMPL(__imp__sub_8232CED8) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x8232CEE0;
	__savegprlr_28(ctx, base);
	// stwu r1,-176(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-176);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// lis r31,-32256
	ctx.r31.s64 = -2113929216;
	// extsw r11,r4
	ctx.r11.s64 = ctx.r4.s32;
	// addi r30,r31,3268
	ctx.r30.s64 = ctx.r31.s64 + 3268;
	// mr r28,r3
	ctx.r28.u64 = ctx.r3.u64;
	// lis r31,-32256
	ctx.r31.s64 = -2113929216;
	// lis r10,-32256
	ctx.r10.s64 = -2113929216;
	// lis r9,-32256
	ctx.r9.s64 = -2113929216;
	// lis r8,-32230
	ctx.r8.s64 = -2112225280;
	// stw r30,128(r1)
	PPC_STORE_U32(ctx.r1.u32 + 128, ctx.r30.u32);
	// lis r7,-32229
	ctx.r7.s64 = -2112159744;
	// lis r6,-32256
	ctx.r6.s64 = -2113929216;
	// lis r5,-32256
	ctx.r5.s64 = -2113929216;
	// lis r4,-32229
	ctx.r4.s64 = -2112159744;
	// lis r3,-32229
	ctx.r3.s64 = -2112159744;
	// addi r29,r31,3276
	ctx.r29.s64 = ctx.r31.s64 + 3276;
	// li r31,0
	ctx.r31.s64 = 0;
	// addi r10,r10,2552
	ctx.r10.s64 = ctx.r10.s64 + 2552;
	// addi r9,r9,2960
	ctx.r9.s64 = ctx.r9.s64 + 2960;
	// addi r8,r8,27512
	ctx.r8.s64 = ctx.r8.s64 + 27512;
	// addi r7,r7,-31812
	ctx.r7.s64 = ctx.r7.s64 + -31812;
	// stw r29,132(r1)
	PPC_STORE_U32(ctx.r1.u32 + 132, ctx.r29.u32);
	// addi r6,r6,3284
	ctx.r6.s64 = ctx.r6.s64 + 3284;
	// stw r31,80(r1)
	PPC_STORE_U32(ctx.r1.u32 + 80, ctx.r31.u32);
	// addi r5,r5,3280
	ctx.r5.s64 = ctx.r5.s64 + 3280;
	// stw r31,84(r1)
	PPC_STORE_U32(ctx.r1.u32 + 84, ctx.r31.u32);
	// addi r4,r4,-31808
	ctx.r4.s64 = ctx.r4.s64 + -31808;
	// stw r31,88(r1)
	PPC_STORE_U32(ctx.r1.u32 + 88, ctx.r31.u32);
	// addi r3,r3,-31804
	ctx.r3.s64 = ctx.r3.s64 + -31804;
	// stw r10,96(r1)
	PPC_STORE_U32(ctx.r1.u32 + 96, ctx.r10.u32);
	// stw r9,100(r1)
	PPC_STORE_U32(ctx.r1.u32 + 100, ctx.r9.u32);
	// mr r29,r31
	ctx.r29.u64 = ctx.r31.u64;
	// stw r8,104(r1)
	PPC_STORE_U32(ctx.r1.u32 + 104, ctx.r8.u32);
	// cmpdi cr6,r11,0
	ctx.cr6.compare<int64_t>(ctx.r11.s64, 0, ctx.xer);
	// stw r7,108(r1)
	PPC_STORE_U32(ctx.r1.u32 + 108, ctx.r7.u32);
	// stw r6,112(r1)
	PPC_STORE_U32(ctx.r1.u32 + 112, ctx.r6.u32);
	// stw r5,116(r1)
	PPC_STORE_U32(ctx.r1.u32 + 116, ctx.r5.u32);
	// stw r4,120(r1)
	PPC_STORE_U32(ctx.r1.u32 + 120, ctx.r4.u32);
	// stw r3,124(r1)
	PPC_STORE_U32(ctx.r1.u32 + 124, ctx.r3.u32);
	// bge cr6,0x8232cf88
	if (!ctx.cr6.lt) goto loc_8232CF88;
	// li r29,1
	ctx.r29.s64 = 1;
	// neg r11,r11
	ctx.r11.s64 = -ctx.r11.s64;
loc_8232CF88:
	// lis r10,26214
	ctx.r10.s64 = 1717960704;
	// lis r9,26214
	ctx.r9.s64 = 1717960704;
	// ori r10,r10,26215
	ctx.r10.u64 = ctx.r10.u64 | 26215;
	// ori r9,r9,26214
	ctx.r9.u64 = ctx.r9.u64 | 26214;
	// addi r8,r1,96
	ctx.r8.s64 = ctx.r1.s64 + 96;
	// rldimi r10,r9,32,0
	ctx.r10.u64 = (__builtin_rotateleft64(ctx.r9.u64, 32) & 0xFFFFFFFF00000000) | (ctx.r10.u64 & 0xFFFFFFFF);
	// extsw r9,r11
	ctx.r9.s64 = ctx.r11.s32;
	// mulhd r11,r11,r10
	ctx.r11.s64 = int64_t((__int128_t(ctx.r11.s64) * __int128_t(ctx.r10.s64)) >> 64);
	// sradi r11,r11,2
	ctx.xer.ca = (ctx.r11.s64 < 0) & ((ctx.r11.u64 & 0x3) != 0);
	ctx.r11.s64 = ctx.r11.s64 >> 2;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// rldicl r10,r11,1,63
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u64, 1) & 0x1;
	// add r30,r11,r10
	ctx.r30.u64 = ctx.r11.u64 + ctx.r10.u64;
	// extsw r11,r30
	ctx.r11.s64 = ctx.r30.s32;
	// rlwinm r10,r11,2,0,29
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
	// add r11,r11,r10
	ctx.r11.u64 = ctx.r11.u64 + ctx.r10.u64;
	// rlwinm r11,r11,1,0,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 1) & 0xFFFFFFFE;
	// subf r11,r11,r9
	ctx.r11.s64 = ctx.r9.s64 - ctx.r11.s64;
	// rlwinm r11,r11,2,0,29
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r4,r11,r8
	ctx.r4.u64 = PPC_LOAD_U32(ctx.r11.u32 + ctx.r8.u32);
	// bl 0x8232d378
	ctx.lr = 0x8232CFD8;
	sub_8232D378(ctx, base);
	// mr r11,r30
	ctx.r11.u64 = ctx.r30.u64;
	// cmpdi cr6,r11,0
	ctx.cr6.compare<int64_t>(ctx.r11.s64, 0, ctx.xer);
	// bne cr6,0x8232cf88
	if (!ctx.cr6.eq) goto loc_8232CF88;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r29.s32, 0, ctx.xer);
	// beq cr6,0x8232cffc
	if (ctx.cr6.eq) goto loc_8232CFFC;
	// lis r11,-32229
	ctx.r11.s64 = -2112159744;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// addi r4,r11,-31800
	ctx.r4.s64 = ctx.r11.s64 + -31800;
	// bl 0x8232d378
	ctx.lr = 0x8232CFFC;
	sub_8232D378(ctx, base);
loc_8232CFFC:
	// addi r4,r1,80
	ctx.r4.s64 = ctx.r1.s64 + 80;
	// mr r3,r28
	ctx.r3.u64 = ctx.r28.u64;
	// bl 0x8232d040
	ctx.lr = 0x8232D008;
	sub_8232D040(ctx, base);
	// lwz r11,88(r1)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r1.u32 + 88);
	// stw r31,84(r1)
	PPC_STORE_U32(ctx.r1.u32 + 84, ctx.r31.u32);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq cr6,0x8232d02c
	if (ctx.cr6.eq) goto loc_8232D02C;
	// li r5,8
	ctx.r5.s64 = 8;
	// stw r31,88(r1)
	PPC_STORE_U32(ctx.r1.u32 + 88, ctx.r31.u32);
	// li r4,2
	ctx.r4.s64 = 2;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x8229f678
	ctx.lr = 0x8232D02C;
	sub_8229F678(ctx, base);
loc_8232D02C:
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x82298a98
	ctx.lr = 0x8232D034;
	sub_82298A98(ctx, base);
	// mr r3,r28
	ctx.r3.u64 = ctx.r28.u64;
	// addi r1,r1,176
	ctx.r1.s64 = ctx.r1.s64 + 176;
	// b 0x82b7a738
	__restgprlr_28(ctx, base);
	return;
}"""),
)


def generate() -> dict:
    source = ROOT / SOURCE
    if not source.exists():
        source = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / SOURCE
    lines = source.read_text(encoding="utf-8").splitlines()
    entries = []
    for address, line, count, frame, first, calls, expected in SPECS:
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
        direct = [value.lower() for value in
                  re.findall(r"// bl 0x([0-9a-f]+)", observed)]
        if first != 31:  # 8232D190 saves LR/r31 inline, without a helper call.
            direct = direct[1:]
        if direct != calls:
            raise ValueError(f"{address} direct-call order differs: {direct}")
        entries.append({
            "address": address, "source": SOURCE.as_posix(),
            "source_line": line, "frame_size": frame,
            "save_first": first, "instructions": instructions,
            "calls": [value.upper() for value in calls],
            "cfg": {
                "labels": re.findall(r"^loc_([0-9A-F]+):", observed, re.MULTILINE),
                "branches": re.findall(r"^[ \t]*// (b[^\n]*)$", observed, re.MULTILINE),
            },
            "translated_body": observed, "status": "bounded_readable_library",
            "boundary": "real mapped direct lower helpers; virtual manager targets external",
        })
    return {"schema": "metadata-utf16-slice-v1", "entries": entries}


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
