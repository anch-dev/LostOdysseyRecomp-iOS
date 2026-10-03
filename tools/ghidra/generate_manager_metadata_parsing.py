"""Admit six exact integer/UTF-16 parsing bodies from fixed PPC locations."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path
from generate_instance_vtable_family import write_if_changed

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "LostOdysseyRecompSemantics/manager_metadata_parsing_families.json"
SPECS = {
    "82296E80": ("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp", 16468, "metadata_numeric_suffix"),
    "822974B0": ("LostOdysseyRecompLib/ppc/ppc_recomp.0.cpp", 17600, "character_class"),
    "82376F98": ("LostOdysseyRecompLib/ppc/ppc_recomp.9.cpp", 21386, "decimal_parse_tail"),
    "82376FA8": ("LostOdysseyRecompLib/ppc/ppc_recomp.9.cpp", 21401, "unicode_digit"),
    "82B7D3E0": ("LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp", 14749, "integer_parse_core"),
    "82B7D688": ("LostOdysseyRecompLib/ppc/ppc_recomp.175.cpp", 15144, "integer_parse_tail"),
}
EXPECTED = {
    "82296E80": """PPC_FUNC_IMPL(__imp__sub_82296E80) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x82296E88;
	__savegprlr_27(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r30,r3
	ctx.r30.u64 = ctx.r3.u64;
	// mr r27,r4
	ctx.r27.u64 = ctx.r4.u64;
	// mr r29,r6
	ctx.r29.u64 = ctx.r6.u64;
	// li r28,0
	ctx.r28.s64 = 0;
	// bl 0x82296830
	ctx.lr = 0x82296EA0;
	sub_82296830(ctx, base);
	// rlwinm r11,r3,1,0,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r3.u32 | (ctx.r3.u64 << 32), 1) & 0xFFFFFFFE;
	// add r11,r11,r30
	ctx.r11.u64 = ctx.r11.u64 + ctx.r30.u64;
	// addi r10,r11,-2
	ctx.r10.s64 = ctx.r11.s64 + -2;
	// mr r31,r10
	ctx.r31.u64 = ctx.r10.u64;
	// lhz r11,0(r10)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + 0);
	// cmplwi cr6,r11,48
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 48, ctx.xer);
	// blt cr6,0x82296f58
	if (ctx.cr6.lt) goto loc_82296F58;
	// cmplwi cr6,r11,57
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 57, ctx.xer);
	// bgt cr6,0x82296f58
	if (ctx.cr6.gt) goto loc_82296F58;
loc_82296EC4:
	// cmplwi cr6,r11,57
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 57, ctx.xer);
	// bgt cr6,0x82296ee4
	if (ctx.cr6.gt) goto loc_82296EE4;
	// cmplw cr6,r31,r30
	ctx.cr6.compare<uint32_t>(ctx.r31.u32, ctx.r30.u32, ctx.xer);
	// ble cr6,0x82296ee4
	if (!ctx.cr6.gt) goto loc_82296EE4;
	// addi r31,r31,-2
	ctx.r31.s64 = ctx.r31.s64 + -2;
	// lhz r11,0(r31)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r31.u32 + 0);
	// cmplwi cr6,r11,48
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 48, ctx.xer);
	// bge cr6,0x82296ec4
	if (!ctx.cr6.lt) goto loc_82296EC4;
loc_82296EE4:
	// lhz r11,0(r31)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r31.u32 + 0);
	// cmplwi cr6,r11,95
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 95, ctx.xer);
	// bne cr6,0x82296f58
	if (!ctx.cr6.eq) goto loc_82296F58;
	// lhz r11,2(r31)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r31.u32 + 2);
	// addi r3,r31,2
	ctx.r3.s64 = ctx.r31.s64 + 2;
	// cmplwi cr6,r11,48
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 48, ctx.xer);
	// bne cr6,0x82296f10
	if (!ctx.cr6.eq) goto loc_82296F10;
	// subf r11,r31,r10
	ctx.r11.s64 = ctx.r10.s64 - ctx.r31.s64;
	// rlwinm r11,r11,0,0,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0xFFFFFFFE;
	// cmpwi cr6,r11,2
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 2, ctx.xer);
	// bne cr6,0x82296f58
	if (!ctx.cr6.eq) goto loc_82296F58;
loc_82296F10:
	// bl 0x82376f98
	ctx.lr = 0x82296F14;
	sub_82376F98(ctx, base);
	// subf r11,r30,r31
	ctx.r11.s64 = ctx.r31.s64 - ctx.r30.s64;
	// stw r3,0(r29)
	PPC_STORE_U32(ctx.r29.u32 + 0, ctx.r3.u32);
	// srawi r11,r11,1
	ctx.xer.ca = (ctx.r11.s32 < 0) & ((ctx.r11.u32 & 0x1) != 0);
	ctx.r11.s64 = ctx.r11.s32 >> 1;
	// addi r31,r11,1
	ctx.r31.s64 = ctx.r11.s64 + 1;
	// cmpwi cr6,r31,128
	ctx.cr6.compare<int32_t>(ctx.r31.s32, 128, ctx.xer);
	// ble cr6,0x82296f30
	if (!ctx.cr6.gt) goto loc_82296F30;
	// li r31,128
	ctx.r31.s64 = 128;
loc_82296F30:
	// mr r5,r31
	ctx.r5.u64 = ctx.r31.u64;
	// mr r4,r30
	ctx.r4.u64 = ctx.r30.u64;
	// mr r3,r27
	ctx.r3.u64 = ctx.r27.u64;
	// bl 0x8232d318
	ctx.lr = 0x82296F40;
	sub_8232D318(ctx, base);
	// rlwinm r11,r31,1,0,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r31.u32 | (ctx.r31.u64 << 32), 1) & 0xFFFFFFFE;
	// li r3,1
	ctx.r3.s64 = 1;
	// add r11,r11,r27
	ctx.r11.u64 = ctx.r11.u64 + ctx.r27.u64;
	// sth r28,-2(r11)
	PPC_STORE_U16(ctx.r11.u32 + -2, ctx.r28.u16);
	// addi r1,r1,128
	ctx.r1.s64 = ctx.r1.s64 + 128;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
loc_82296F58:
	// mr r3,r28
	ctx.r3.u64 = ctx.r28.u64;
	// addi r1,r1,128
	ctx.r1.s64 = ctx.r1.s64 + 128;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
}
""",
    "822974B0": """PPC_FUNC_IMPL(__imp__sub_822974B0) {
	PPC_FUNC_PROLOGUE();
	// clrlwi r11,r3,16
	ctx.r11.u64 = ctx.r3.u32 & 0xFFFF;
	// cmplwi cr6,r11,65535
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 65535, ctx.xer);
	// beq cr6,0x822974e0
	if (ctx.cr6.eq) goto loc_822974E0;
	// cmplwi cr6,r11,256
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 256, ctx.xer);
	// bge cr6,0x822974e0
	if (!ctx.cr6.lt) goto loc_822974E0;
	// rlwinm r10,r11,1,0,30
	ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 1) & 0xFFFFFFFE;
	// lis r11,-31967
	ctx.r11.s64 = -2094989312;
	// clrlwi r9,r4,16
	ctx.r9.u64 = ctx.r4.u32 & 0xFFFF;
	// lwz r11,23360(r11)
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r11.u32 + 23360);
	// lhzx r11,r10,r11
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r10.u32 + ctx.r11.u32);
	// and r11,r11,r9
	ctx.r11.u64 = ctx.r11.u64 & ctx.r9.u64;
	// b 0x822974e4
	goto loc_822974E4;
loc_822974E0:
	// li r11,0
	ctx.r11.s64 = 0;
loc_822974E4:
	// clrlwi r11,r11,16
	ctx.r11.u64 = ctx.r11.u32 & 0xFFFF;
	// clrlwi r10,r4,16
	ctx.r10.u64 = ctx.r4.u32 & 0xFFFF;
	// and r3,r11,r10
	ctx.r3.u64 = ctx.r11.u64 & ctx.r10.u64;
	// blr
	return;
}
""",
    "82376F98": """PPC_FUNC_IMPL(__imp__sub_82376F98) {
	PPC_FUNC_PROLOGUE();
	// li r5,10
	ctx.r5.s64 = 10;
	// li r4,0
	ctx.r4.s64 = 0;
	// b 0x82b7d688
	sub_82B7D688(ctx, base);
	return;
}
""",
    "82376FA8": """PPC_FUNC_IMPL(__imp__sub_82376FA8) {
	PPC_FUNC_PROLOGUE();
	// clrlwi r11,r3,16
	ctx.r11.u64 = ctx.r3.u32 & 0xFFFF;
	// cmplwi cr6,r11,48
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 48, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,58
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 58, ctx.xer);
	// bge cr6,0x82376fc4
	if (!ctx.cr6.lt) goto loc_82376FC4;
	// addi r3,r11,-48
	ctx.r3.s64 = ctx.r11.s64 + -48;
	// blr
	return;
loc_82376FC4:
	// cmplwi cr6,r11,65296
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 65296, ctx.xer);
	// bge cr6,0x8237714c
	if (!ctx.cr6.lt) goto loc_8237714C;
	// cmplwi cr6,r11,1632
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1632, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,1642
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1642, ctx.xer);
	// bge cr6,0x82376fe4
	if (!ctx.cr6.lt) goto loc_82376FE4;
	// addi r3,r11,-1632
	ctx.r3.s64 = ctx.r11.s64 + -1632;
	// blr
	return;
loc_82376FE4:
	// cmplwi cr6,r11,1776
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1776, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,1786
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 1786, ctx.xer);
	// bge cr6,0x82376ffc
	if (!ctx.cr6.lt) goto loc_82376FFC;
	// addi r3,r11,-1776
	ctx.r3.s64 = ctx.r11.s64 + -1776;
	// blr
	return;
loc_82376FFC:
	// cmplwi cr6,r11,2406
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2406, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,2416
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2416, ctx.xer);
	// bge cr6,0x82377014
	if (!ctx.cr6.lt) goto loc_82377014;
	// addi r3,r11,-2406
	ctx.r3.s64 = ctx.r11.s64 + -2406;
	// blr
	return;
loc_82377014:
	// cmplwi cr6,r11,2534
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2534, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,2544
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2544, ctx.xer);
	// bge cr6,0x8237702c
	if (!ctx.cr6.lt) goto loc_8237702C;
	// addi r3,r11,-2534
	ctx.r3.s64 = ctx.r11.s64 + -2534;
	// blr
	return;
loc_8237702C:
	// cmplwi cr6,r11,2662
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2662, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,2672
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2672, ctx.xer);
	// bge cr6,0x82377044
	if (!ctx.cr6.lt) goto loc_82377044;
	// addi r3,r11,-2662
	ctx.r3.s64 = ctx.r11.s64 + -2662;
	// blr
	return;
loc_82377044:
	// cmplwi cr6,r11,2790
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2790, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,2800
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2800, ctx.xer);
	// bge cr6,0x8237705c
	if (!ctx.cr6.lt) goto loc_8237705C;
	// addi r3,r11,-2790
	ctx.r3.s64 = ctx.r11.s64 + -2790;
	// blr
	return;
loc_8237705C:
	// cmplwi cr6,r11,2918
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2918, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,2928
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 2928, ctx.xer);
	// bge cr6,0x82377074
	if (!ctx.cr6.lt) goto loc_82377074;
	// addi r3,r11,-2918
	ctx.r3.s64 = ctx.r11.s64 + -2918;
	// blr
	return;
loc_82377074:
	// cmplwi cr6,r11,3174
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3174, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,3184
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3184, ctx.xer);
	// bge cr6,0x8237708c
	if (!ctx.cr6.lt) goto loc_8237708C;
	// addi r3,r11,-3174
	ctx.r3.s64 = ctx.r11.s64 + -3174;
	// blr
	return;
loc_8237708C:
	// cmplwi cr6,r11,3302
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3302, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,3312
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3312, ctx.xer);
	// bge cr6,0x823770a4
	if (!ctx.cr6.lt) goto loc_823770A4;
	// addi r3,r11,-3302
	ctx.r3.s64 = ctx.r11.s64 + -3302;
	// blr
	return;
loc_823770A4:
	// cmplwi cr6,r11,3430
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3430, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,3440
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3440, ctx.xer);
	// bge cr6,0x823770bc
	if (!ctx.cr6.lt) goto loc_823770BC;
	// addi r3,r11,-3430
	ctx.r3.s64 = ctx.r11.s64 + -3430;
	// blr
	return;
loc_823770BC:
	// cmplwi cr6,r11,3664
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3664, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,3674
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3674, ctx.xer);
	// bge cr6,0x823770d4
	if (!ctx.cr6.lt) goto loc_823770D4;
	// addi r3,r11,-3664
	ctx.r3.s64 = ctx.r11.s64 + -3664;
	// blr
	return;
loc_823770D4:
	// cmplwi cr6,r11,3792
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3792, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,3802
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3802, ctx.xer);
	// bge cr6,0x823770ec
	if (!ctx.cr6.lt) goto loc_823770EC;
	// addi r3,r11,-3792
	ctx.r3.s64 = ctx.r11.s64 + -3792;
	// blr
	return;
loc_823770EC:
	// cmplwi cr6,r11,3872
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3872, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,3882
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 3882, ctx.xer);
	// bge cr6,0x82377104
	if (!ctx.cr6.lt) goto loc_82377104;
	// addi r3,r11,-3872
	ctx.r3.s64 = ctx.r11.s64 + -3872;
	// blr
	return;
loc_82377104:
	// cmplwi cr6,r11,4160
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 4160, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,4170
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 4170, ctx.xer);
	// bge cr6,0x8237711c
	if (!ctx.cr6.lt) goto loc_8237711C;
	// addi r3,r11,-4160
	ctx.r3.s64 = ctx.r11.s64 + -4160;
	// blr
	return;
loc_8237711C:
	// cmplwi cr6,r11,6112
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 6112, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,6122
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 6122, ctx.xer);
	// bge cr6,0x82377134
	if (!ctx.cr6.lt) goto loc_82377134;
	// addi r3,r11,-6112
	ctx.r3.s64 = ctx.r11.s64 + -6112;
	// blr
	return;
loc_82377134:
	// cmplwi cr6,r11,6160
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 6160, ctx.xer);
	// blt cr6,0x82377160
	if (ctx.cr6.lt) goto loc_82377160;
	// cmplwi cr6,r11,6170
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 6170, ctx.xer);
	// bge cr6,0x82377160
	if (!ctx.cr6.lt) goto loc_82377160;
	// addi r3,r11,-6160
	ctx.r3.s64 = ctx.r11.s64 + -6160;
	// blr
	return;
loc_8237714C:
	// cmplwi cr6,r11,65306
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 65306, ctx.xer);
	// bge cr6,0x82377160
	if (!ctx.cr6.lt) goto loc_82377160;
	// addis r3,r11,-1
	ctx.r3.s64 = ctx.r11.s64 + -65536;
	// addi r3,r3,240
	ctx.r3.s64 = ctx.r3.s64 + 240;
	// blr
	return;
loc_82377160:
	// li r3,-1
	ctx.r3.s64 = -1;
	// blr
	return;
}
""",
    "82B7D3E0": """PPC_FUNC_IMPL(__imp__sub_82B7D3E0) {
	PPC_FUNC_PROLOGUE();
	PPCRegister temp{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82b7a6d4
	ctx.lr = 0x82B7D3E8;
	__savegprlr_23(ctx, base);
	// stwu r1,-160(r1)
	temp.u64 = ctx.r1.u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r1.u32);
	ctx.r1.u64 = temp.u64;
	// mr r23,r5
	ctx.r23.u64 = ctx.r5.u64;
	// mr r25,r4
	ctx.r25.u64 = ctx.r4.u64;
	// mr r28,r6
	ctx.r28.u64 = ctx.r6.u64;
	// mr r24,r7
	ctx.r24.u64 = ctx.r7.u64;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r23.u32, 0, ctx.xer);
	// beq cr6,0x82b7d408
	if (ctx.cr6.eq) goto loc_82B7D408;
	// stw r25,0(r23)
	PPC_STORE_U32(ctx.r23.u32 + 0, ctx.r25.u32);
loc_82B7D408:
	// cmplwi cr6,r25,0
	ctx.cr6.compare<uint32_t>(ctx.r25.u32, 0, ctx.xer);
	// bne cr6,0x82b7d440
	if (!ctx.cr6.eq) goto loc_82B7D440;
loc_82B7D410:
	// bl 0x82b7fd78
	ctx.lr = 0x82B7D414;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r11.u64 = ctx.r3.u64;
	// li r10,22
	ctx.r10.s64 = 22;
	// li r7,0
	ctx.r7.s64 = 0;
	// li r6,0
	ctx.r6.s64 = 0;
	// li r5,0
	ctx.r5.s64 = 0;
	// li r4,0
	ctx.r4.s64 = 0;
	// li r3,0
	ctx.r3.s64 = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r11.u32 + 0, ctx.r10.u32);
	// bl 0x82b7fec0
	ctx.lr = 0x82B7D438;
	sub_82B7FEC0(ctx, base);
	// li r3,0
	ctx.r3.s64 = 0;
	// b 0x82b7d67c
	goto loc_82B7D67C;
loc_82B7D440:
	// cmpwi cr6,r28,0
	ctx.cr6.compare<int32_t>(ctx.r28.s32, 0, ctx.xer);
	// beq cr6,0x82b7d458
	if (ctx.cr6.eq) goto loc_82B7D458;
	// cmpwi cr6,r28,2
	ctx.cr6.compare<int32_t>(ctx.r28.s32, 2, ctx.xer);
	// blt cr6,0x82b7d410
	if (ctx.cr6.lt) goto loc_82B7D410;
	// cmpwi cr6,r28,36
	ctx.cr6.compare<int32_t>(ctx.r28.s32, 36, ctx.xer);
	// bgt cr6,0x82b7d410
	if (ctx.cr6.gt) goto loc_82B7D410;
loc_82B7D458:
	// lis r11,-31967
	ctx.r11.s64 = -2094989312;
	// lhz r31,0(r25)
	ctx.r31.u64 = PPC_LOAD_U16(ctx.r25.u32 + 0);
	// li r27,0
	ctx.r27.s64 = 0;
	// addi r30,r11,21248
	ctx.r30.s64 = ctx.r11.s64 + 21248;
	// addi r29,r25,2
	ctx.r29.s64 = ctx.r25.s64 + 2;
	// b 0x82b7d478
	goto loc_82B7D478;
loc_82B7D470:
	// lhz r31,0(r29)
	ctx.r31.u64 = PPC_LOAD_U16(ctx.r29.u32 + 0);
	// addi r29,r29,2
	ctx.r29.s64 = ctx.r29.s64 + 2;
loc_82B7D478:
	// mr r5,r30
	ctx.r5.u64 = ctx.r30.u64;
	// li r4,8
	ctx.r4.s64 = 8;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x822974b0
	ctx.lr = 0x82B7D488;
	sub_822974B0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// bne 0x82b7d470
	if (!ctx.cr0.eq) goto loc_82B7D470;
	// clrlwi r11,r31,16
	ctx.r11.u64 = ctx.r31.u32 & 0xFFFF;
	// cmplwi cr6,r11,45
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 45, ctx.xer);
	// bne cr6,0x82b7d4a4
	if (!ctx.cr6.eq) goto loc_82B7D4A4;
	// ori r24,r24,2
	ctx.r24.u64 = ctx.r24.u64 | 2;
	// b 0x82b7d4ac
	goto loc_82B7D4AC;
loc_82B7D4A4:
	// cmplwi cr6,r11,43
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 43, ctx.xer);
	// bne cr6,0x82b7d4b4
	if (!ctx.cr6.eq) goto loc_82B7D4B4;
loc_82B7D4AC:
	// lhz r31,0(r29)
	ctx.r31.u64 = PPC_LOAD_U16(ctx.r29.u32 + 0);
	// addi r29,r29,2
	ctx.r29.s64 = ctx.r29.s64 + 2;
loc_82B7D4B4:
	// cmpwi cr6,r28,0
	ctx.cr6.compare<int32_t>(ctx.r28.s32, 0, ctx.xer);
	// bne cr6,0x82b7d4f4
	if (!ctx.cr6.eq) goto loc_82B7D4F4;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82376fa8
	ctx.lr = 0x82B7D4C4;
	sub_82376FA8(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// beq 0x82b7d4d4
	if (ctx.cr0.eq) goto loc_82B7D4D4;
	// li r28,10
	ctx.r28.s64 = 10;
	// b 0x82b7d52c
	goto loc_82B7D52C;
loc_82B7D4D4:
	// lhz r11,0(r29)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r29.u32 + 0);
	// cmplwi cr6,r11,120
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 120, ctx.xer);
	// beq cr6,0x82b7d4f0
	if (ctx.cr6.eq) goto loc_82B7D4F0;
	// cmplwi cr6,r11,88
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 88, ctx.xer);
	// beq cr6,0x82b7d4f0
	if (ctx.cr6.eq) goto loc_82B7D4F0;
	// li r28,8
	ctx.r28.s64 = 8;
	// b 0x82b7d52c
	goto loc_82B7D52C;
loc_82B7D4F0:
	// li r28,16
	ctx.r28.s64 = 16;
loc_82B7D4F4:
	// cmpwi cr6,r28,16
	ctx.cr6.compare<int32_t>(ctx.r28.s32, 16, ctx.xer);
	// bne cr6,0x82b7d52c
	if (!ctx.cr6.eq) goto loc_82B7D52C;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82376fa8
	ctx.lr = 0x82B7D504;
	sub_82376FA8(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
	// bne 0x82b7d52c
	if (!ctx.cr0.eq) goto loc_82B7D52C;
	// lhz r11,0(r29)
	ctx.r11.u64 = PPC_LOAD_U16(ctx.r29.u32 + 0);
	// cmplwi cr6,r11,120
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 120, ctx.xer);
	// beq cr6,0x82b7d520
	if (ctx.cr6.eq) goto loc_82B7D520;
	// cmplwi cr6,r11,88
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 88, ctx.xer);
	// bne cr6,0x82b7d52c
	if (!ctx.cr6.eq) goto loc_82B7D52C;
loc_82B7D520:
	// addi r11,r29,2
	ctx.r11.s64 = ctx.r29.s64 + 2;
	// addi r29,r11,2
	ctx.r29.s64 = ctx.r11.s64 + 2;
	// lhz r31,0(r11)
	ctx.r31.u64 = PPC_LOAD_U16(ctx.r11.u32 + 0);
loc_82B7D52C:
	// li r26,-1
	ctx.r26.s64 = -1;
	// twllei r28,0
	// divwu r30,r26,r28
	ctx.r30.u32 = ctx.r26.u32 / ctx.r28.u32;
loc_82B7D538:
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82376fa8
	ctx.lr = 0x82B7D540;
	sub_82376FA8(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r3.s32, -1, ctx.xer);
	// bne cr6,0x82b7d584
	if (!ctx.cr6.eq) goto loc_82B7D584;
	// clrlwi r11,r31,16
	ctx.r11.u64 = ctx.r31.u32 & 0xFFFF;
	// cmplwi cr6,r11,65
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 65, ctx.xer);
	// blt cr6,0x82b7d55c
	if (ctx.cr6.lt) goto loc_82B7D55C;
	// cmplwi cr6,r11,90
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 90, ctx.xer);
	// ble cr6,0x82b7d56c
	if (!ctx.cr6.gt) goto loc_82B7D56C;
loc_82B7D55C:
	// cmplwi cr6,r11,97
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 97, ctx.xer);
	// blt cr6,0x82b7d5c4
	if (ctx.cr6.lt) goto loc_82B7D5C4;
	// cmplwi cr6,r11,122
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 122, ctx.xer);
	// bgt cr6,0x82b7d5c4
	if (ctx.cr6.gt) goto loc_82B7D5C4;
loc_82B7D56C:
	// cmplwi cr6,r11,97
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 97, ctx.xer);
	// blt cr6,0x82b7d580
	if (ctx.cr6.lt) goto loc_82B7D580;
	// cmplwi cr6,r11,122
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 122, ctx.xer);
	// bgt cr6,0x82b7d580
	if (ctx.cr6.gt) goto loc_82B7D580;
	// addi r11,r11,-32
	ctx.r11.s64 = ctx.r11.s64 + -32;
loc_82B7D580:
	// addi r3,r11,-55
	ctx.r3.s64 = ctx.r11.s64 + -55;
loc_82B7D584:
	// cmplw cr6,r3,r28
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, ctx.r28.u32, ctx.xer);
	// bge cr6,0x82b7d5c4
	if (!ctx.cr6.lt) goto loc_82B7D5C4;
	// ori r24,r24,8
	ctx.r24.u64 = ctx.r24.u64 | 8;
	// cmplw cr6,r27,r30
	ctx.cr6.compare<uint32_t>(ctx.r27.u32, ctx.r30.u32, ctx.xer);
	// blt cr6,0x82b7d5e4
	if (ctx.cr6.lt) goto loc_82B7D5E4;
	// bne cr6,0x82b7d5b8
	if (!ctx.cr6.eq) goto loc_82B7D5B8;
	// mr r11,r26
	ctx.r11.u64 = ctx.r26.u64;
	// twllei r28,0
	// divwu r10,r11,r28
	ctx.r10.u32 = ctx.r11.u32 / ctx.r28.u32;
	// mullw r10,r10,r28
	ctx.r10.s64 = int64_t(ctx.r10.s32) * int64_t(ctx.r28.s32);
	// subf r11,r10,r11
	ctx.r11.s64 = ctx.r11.s64 - ctx.r10.s64;
	// cmplw cr6,r3,r11
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, ctx.r11.u32, ctx.xer);
	// ble cr6,0x82b7d5e4
	if (!ctx.cr6.gt) goto loc_82B7D5E4;
loc_82B7D5B8:
	// ori r24,r24,4
	ctx.r24.u64 = ctx.r24.u64 | 4;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r23.u32, 0, ctx.xer);
	// bne cr6,0x82b7d5ec
	if (!ctx.cr6.eq) goto loc_82B7D5EC;
loc_82B7D5C4:
	// rlwinm. r11,r24,0,28,28
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r24.u32 | (ctx.r24.u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// addi r29,r29,-2
	ctx.r29.s64 = ctx.r29.s64 + -2;
	// bne 0x82b7d5f8
	if (!ctx.cr0.eq) goto loc_82B7D5F8;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r23.u32, 0, ctx.xer);
	// beq cr6,0x82b7d5dc
	if (ctx.cr6.eq) goto loc_82B7D5DC;
	// mr r29,r25
	ctx.r29.u64 = ctx.r25.u64;
loc_82B7D5DC:
	// li r27,0
	ctx.r27.s64 = 0;
	// b 0x82b7d660
	goto loc_82B7D660;
loc_82B7D5E4:
	// mullw r11,r27,r28
	ctx.r11.s64 = int64_t(ctx.r27.s32) * int64_t(ctx.r28.s32);
	// add r27,r11,r3
	ctx.r27.u64 = ctx.r11.u64 + ctx.r3.u64;
loc_82B7D5EC:
	// lhz r31,0(r29)
	ctx.r31.u64 = PPC_LOAD_U16(ctx.r29.u32 + 0);
	// addi r29,r29,2
	ctx.r29.s64 = ctx.r29.s64 + 2;
	// b 0x82b7d538
	goto loc_82B7D538;
loc_82B7D5F8:
	// lis r10,32767
	ctx.r10.s64 = 2147418112;
	// rlwinm. r11,r24,0,29,29
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r24.u32 | (ctx.r24.u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// ori r31,r10,65535
	ctx.r31.u64 = ctx.r10.u64 | 65535;
	// lis r30,-32768
	ctx.r30.s64 = -2147483648;
	// bne 0x82b7d634
	if (!ctx.cr0.eq) goto loc_82B7D634;
	// clrlwi. r11,r24,31
	ctx.r11.u64 = ctx.r24.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82b7d660
	if (!ctx.cr0.eq) goto loc_82B7D660;
	// rlwinm. r11,r24,0,30,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r24.u32 | (ctx.r24.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82b7d624
	if (ctx.cr0.eq) goto loc_82B7D624;
	// cmplw cr6,r27,r30
	ctx.cr6.compare<uint32_t>(ctx.r27.u32, ctx.r30.u32, ctx.xer);
	// bgt cr6,0x82b7d634
	if (ctx.cr6.gt) goto loc_82B7D634;
loc_82B7D624:
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne cr6,0x82b7d660
	if (!ctx.cr6.eq) goto loc_82B7D660;
	// cmplw cr6,r27,r31
	ctx.cr6.compare<uint32_t>(ctx.r27.u32, ctx.r31.u32, ctx.xer);
	// ble cr6,0x82b7d660
	if (!ctx.cr6.gt) goto loc_82B7D660;
loc_82B7D634:
	// bl 0x82b7fd78
	ctx.lr = 0x82B7D638;
	sub_82B7FD78(ctx, base);
	// li r11,34
	ctx.r11.s64 = 34;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r3.u32 + 0, ctx.r11.u32);
	// clrlwi. r10,r24,31
	ctx.r10.u64 = ctx.r24.u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	// beq 0x82b7d650
	if (ctx.cr0.eq) goto loc_82B7D650;
	// mr r27,r26
	ctx.r27.u64 = ctx.r26.u64;
	// b 0x82b7d660
	goto loc_82B7D660;
loc_82B7D650:
	// rlwinm. r11,r24,0,30,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r24.u32 | (ctx.r24.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// mr r27,r30
	ctx.r27.u64 = ctx.r30.u64;
	// bne 0x82b7d660
	if (!ctx.cr0.eq) goto loc_82B7D660;
	// mr r27,r31
	ctx.r27.u64 = ctx.r31.u64;
loc_82B7D660:
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r23.u32, 0, ctx.xer);
	// beq cr6,0x82b7d66c
	if (ctx.cr6.eq) goto loc_82B7D66C;
	// stw r29,0(r23)
	PPC_STORE_U32(ctx.r23.u32 + 0, ctx.r29.u32);
loc_82B7D66C:
	// rlwinm. r11,r24,0,30,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r24.u32 | (ctx.r24.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// beq 0x82b7d678
	if (ctx.cr0.eq) goto loc_82B7D678;
	// neg r27,r27
	ctx.r27.s64 = -ctx.r27.s64;
loc_82B7D678:
	// mr r3,r27
	ctx.r3.u64 = ctx.r27.u64;
loc_82B7D67C:
	// addi r1,r1,160
	ctx.r1.s64 = ctx.r1.s64 + 160;
	// b 0x82b7a724
	__restgprlr_23(ctx, base);
	return;
}
""",
    "82B7D688": """PPC_FUNC_IMPL(__imp__sub_82B7D688) {
	PPC_FUNC_PROLOGUE();
	// mr r11,r3
	ctx.r11.u64 = ctx.r3.u64;
	// lis r10,-31967
	ctx.r10.s64 = -2094989312;
	// mr r6,r5
	ctx.r6.u64 = ctx.r5.u64;
	// mr r5,r4
	ctx.r5.u64 = ctx.r4.u64;
	// addi r3,r10,21248
	ctx.r3.s64 = ctx.r10.s64 + 21248;
	// li r7,0
	ctx.r7.s64 = 0;
	// mr r4,r11
	ctx.r4.u64 = ctx.r11.u64;
	// b 0x82b7d3e0
	sub_82B7D3E0(ctx, base);
	return;
}
""",
}
EXPECTED_CALLS = {
    "82296E80": ["82b7a6e4", "82296830", "82376f98", "8232d318"],
    "822974B0": [],
    "82376F98": [],
    "82376FA8": [],
    "82B7D3E0": ["82b7a6d4", "82b7fd78", "82b7fec0", "822974b0",
                   "82376fa8", "82376fa8", "82376fa8", "82b7fd78"],
    "82B7D688": [],
}
EXPECTED_TAILS = {"82376F98": "82b7d688", "82B7D688": "82b7d3e0"}

def normalize(body: str) -> list[str]:
    return [line.strip() for line in body.splitlines() if line.strip()]

def read_fixed_source(path: Path, line: int) -> str:
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    start = line - 1
    if start < 0 or start >= len(lines):
        raise ValueError(f"fixed PPC source line moved: {path}:{line}")
    end = start + 1
    while end < len(lines) and lines[end].strip() != "}":
        if lines[end].startswith("PPC_FUNC_IMPL("):
            raise ValueError("unterminated original PPC body")
        end += 1
    if end == len(lines):
        raise ValueError("unterminated original PPC body")
    return "".join(lines[start:end + 1])

def generate() -> dict:
    entries = []
    for address, (source, line, kind) in SPECS.items():
        cached = (ROOT / "out/function-inventory" /
                  f"manager-object-registration-{address}.cpp").read_text(encoding="utf-8")
        source_path = ROOT / source
        if not source_path.exists():
            source_path = Path.home() / "ownCloud/Git/LostOdysseyRecomp" / source
        original = read_fixed_source(source_path, line) if source_path.exists() else cached
        if normalize(cached) != normalize(EXPECTED[address]) or \
                normalize(original) != normalize(EXPECTED[address]):
            raise ValueError(f"{address}: complete translated body/CFG changed")
        instructions = re.findall(r"^\s*//\s*(.*?)\s*$", original, re.M)
        calls = re.findall(r"^\s*// bl 0x([0-9a-f]+)$", original, re.M)
        if calls != EXPECTED_CALLS[address]:
            raise ValueError(f"{address}: direct calls changed")
        tails = re.findall(r"^\s*// b 0x([0-9a-f]+)$", original, re.M)
        if address in EXPECTED_TAILS and EXPECTED_TAILS[address] not in tails:
            raise ValueError(f"{address}: tail target changed")
        entries.append({"address": address, "source": source,
                        "source_line": line, "kind": kind,
                        "instruction_sequence": instructions,
                        "translated_body": original,
                        "cfg": [part for part in instructions if part.startswith(("b", "mtctr"))],
                        "direct_calls": calls,
                        "tail_target": EXPECTED_TAILS.get(address),
                        "bounded_status": "recovered_with_reused_lower_semantics"})
    return {"schema_version": 1, "entry_count": 6, "entries": entries,
            "bounded_effects": "Full original bodies and call order, ordinary big-endian guest memory, ordered UTF16 classification, integer parse and suffix copy, own ABI saves plus full r13. The generated C++ mullw translation uses a full signed 64-bit product; this comparison does not establish hardware PPC behavior for that intermediate. Existing CRT-thread-data, errno, invalid-parameter, UTF16 length and padded-copy models are reused. TLS/handler callbacks and generic volatile ABI remain explicit boundaries."}

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    encoded = json.dumps(generate(), indent=2) + "\n"
    if args.write:
        write_if_changed(MANIFEST, encoded)
    elif MANIFEST.read_text(encoding="utf-8") != encoded:
        raise ValueError("manager metadata parsing manifest changed")
    print("PASS manager metadata parsing 6 complete bodies/CFG/calls")

if __name__ == "__main__":
    main()
