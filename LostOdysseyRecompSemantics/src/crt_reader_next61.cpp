#include "lo_semantics/crt_reader_next61.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_reader_next61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Direct(GuestAddress entry,Context& ctx,Base& base) {
    ToFull(base.full,ctx);
    const bool found=entry==0x82b81360u
        ?crt_stream_byte_read_context::Apply(entry,base.memory,base.dependencies,base.full)
        :crt_stream_close_shared_lower::ApplyAcceptedLower(entry,base.memory,base.dependencies,base.full);
    if(!found)throw std::logic_error("missing accepted next reader lower");
    FromFull(ctx,base.full);
}
void __imp__RtlEnterCriticalSection(Context& ctx,Base& base) {
    ToFull(base.full,ctx);base.dependencies.native.EnterCriticalSection(base.memory,base.full);
    FromFull(ctx,base.full);
}
void __savegprlr_24(Context& c,Base& b){Save(24,c,b);}
void __restgprlr_24(Context& c,Base& b){Restore(24,c,b);}
void Body_82B7B2F8(Context&,Base&);
void Body_82B7B564(Context&,Base&);
void sub_82B7B564(Context& c,Base& b){Body_82B7B564(c,b);}
void Body_82B7B708(Context&,Base&);
void sub_82B7B708(Context& c,Base& b){Body_82B7B708(c,b);}
void Body_82B81648(Context&,Base&);
void sub_82B81648(Context& c,Base& b){Body_82B81648(c,b);}
void sub_82B7FD78(Context& c,Base& b){Direct(0x82b7fd78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b){Direct(0x82b7fec0u,c,b);}
void sub_82B81B28(Context& c,Base& b){Direct(0x82b81b28u,c,b);}
void sub_82B7B7C8(Context& c,Base& b){Direct(0x82b7b7c8u,c,b);}
void sub_82B81360(Context& c,Base& b){Direct(0x82b81360u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82B7B2F8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6d8
	ctx.lr = 0x82B7B300;
	__savegprlr_24(ctx, base);
	// addi r31,r1,-176
	ctx.r[31].s64 = ctx.r[1].s64 + -176;
	// stwu r1,-176(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-176);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r26,r3
	ctx.r[26].u64 = ctx.r[3].u64;
	// mr r24,r4
	ctx.r[24].u64 = ctx.r[4].u64;
	// mr r30,r5
	ctx.r[30].u64 = ctx.r[5].u64;
	// mr r25,r26
	ctx.r[25].u64 = ctx.r[26].u64;
	// mr r27,r26
	ctx.r[27].u64 = ctx.r[26].u64;
	// stw r27,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[27].u32);
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// bne cr6,0x82b7b360
	if (!ctx.cr6.eq) goto loc_82B7B360;
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// beq cr6,0x82b7b360
	if (ctx.cr6.eq) goto loc_82B7B360;
loc_82B7B330:
	// bl 0x82b7fd78
	ctx.lr = 0x82B7B334;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,22
	ctx.r[10].s64 = 22;
	// li r7,0
	ctx.r[7].s64 = 0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,0
	ctx.r[5].s64 = 0;
	// li r4,0
	ctx.r[4].s64 = 0;
	// li r3,0
	ctx.r[3].s64 = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// bl 0x82b7fec0
	ctx.lr = 0x82B7B358;
	sub_82B7FEC0(ctx, base);
loc_82B7B358:
	// li r3,0
	ctx.r[3].s64 = 0;
	// b 0x82b7b53c
	goto loc_82B7B53C;
loc_82B7B360:
	// cntlzw r11,r24
	ctx.r[11].u64 = ctx.r[24].u32 == 0 ? 32 : std::countl_zero(ctx.r[24].u32);
	// cntlzw r11,r11
	ctx.r[11].u64 = ctx.r[11].u32 == 0 ? 32 : std::countl_zero(ctx.r[11].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b7b330
	if (ctx.cr0.eq) goto loc_82B7B330;
	// cntlzw r11,r30
	ctx.r[11].u64 = ctx.r[30].u32 == 0 ? 32 : std::countl_zero(ctx.r[30].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b7b330
	if (ctx.cr0.eq) goto loc_82B7B330;
	// cmpwi cr6,r24,0
	ctx.cr6.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// beq cr6,0x82b7b358
	if (ctx.cr6.eq) goto loc_82B7B358;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// stw r30,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[30].u32);
	// bl 0x82b7b708
	ctx.lr = 0x82B7B3A0;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwz r11,12(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 12);
	// rlwinm. r11,r11,0,25,25
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b7b4a8
	if (!ctx.cr0.eq) goto loc_82B7B4A8;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B3B8;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82b7b408
	if (ctx.cr6.eq) goto loc_82B7B408;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B3C8;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82b7b408
	if (ctx.cr6.eq) goto loc_82B7B408;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B3D8;
	sub_82B81648(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// rlwinm r28,r11,2,0,29
	ctx.r[28].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B3F0;
	sub_82B81648(ctx, base);
	// lwzx r10,r28,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r10,r11,r10
	ctx.r[10].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r28,r11,21272
	ctx.r[28].s64 = ctx.r[11].s64 + 21272;
	// b 0x82b7b41c
	goto loc_82B7B41C;
loc_82B7B408:
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r28,r11,21272
	ctx.r[28].s64 = ctx.r[11].s64 + 21272;
	// mr r10,r28
	ctx.r[10].u64 = ctx.r[28].u64;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
loc_82B7B41C:
	// lbz r11,40(r10)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[10].u32 + 40);
	// rlwinm. r11,r11,0,0,30
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b7b478
	if (!ctx.cr0.eq) goto loc_82B7B478;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B430;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82b7b46c
	if (ctx.cr6.eq) goto loc_82B7B46C;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B440;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82b7b46c
	if (ctx.cr6.eq) goto loc_82B7B46C;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B450;
	sub_82B81648(ctx, base);
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[3].s32 >> 5;
	// rlwinm r28,r11,2,0,29
	ctx.r[28].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B7B460;
	sub_82B81648(ctx, base);
	// lwzx r10,r28,r29
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[29].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r28,r11,r10
	ctx.r[28].u64 = ctx.r[11].u64 + ctx.r[10].u64;
loc_82B7B46C:
	// lbz r11,40(r28)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[28].u32 + 40);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b7b4a8
	if (ctx.cr0.eq) goto loc_82B7B4A8;
loc_82B7B478:
	// bl 0x82b7fd78
	ctx.lr = 0x82B7B47C;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,22
	ctx.r[10].s64 = 22;
	// li r7,0
	ctx.r[7].s64 = 0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,0
	ctx.r[5].s64 = 0;
	// li r4,0
	ctx.r[4].s64 = 0;
	// li r3,0
	ctx.r[3].s64 = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// bl 0x82b7fec0
	ctx.lr = 0x82B7B4A0;
	sub_82B7FEC0(ctx, base);
	// li r27,0
	ctx.r[27].s64 = 0;
	// stw r27,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[27].u32);
loc_82B7B4A8:
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x82b7b52c
	if (ctx.cr6.eq) goto loc_82B7B52C;
loc_82B7B4B0:
	// addic. r24,r24,-1
	ctx.xer.ca = ctx.r[24].u32 > 0;
	ctx.r[24].s64 = ctx.r[24].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[24].s32, 0, ctx.xer);
	// stw r24,204(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 204, ctx.r[24].u32);
	// beq 0x82b7b524
	if (ctx.cr0.eq) goto loc_82B7B524;
	// lwz r11,4(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 4);
	// addic. r11,r11,-1
	ctx.xer.ca = ctx.r[11].u32 > 0;
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// stw r11,4(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 4, ctx.r[11].u32);
	// blt 0x82b7b4e0
	if (ctx.cr0.lt) goto loc_82B7B4E0;
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// addi r10,r11,1
	ctx.r[10].s64 = ctx.r[11].s64 + 1;
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// stw r10,0(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 0, ctx.r[10].u32);
	// b 0x82b7b4e8
	goto loc_82B7B4E8;
loc_82B7B4E0:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81360
	ctx.lr = 0x82B7B4E8;
	sub_82B81360(ctx, base);
loc_82B7B4E8:
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// bne cr6,0x82b7b504
	if (!ctx.cr6.eq) goto loc_82B7B504;
	// cmplw cr6,r25,r26
	ctx.cr6.compare<uint32_t>(ctx.r[25].u32, ctx.r[26].u32, ctx.xer);
	// bne cr6,0x82b7b524
	if (!ctx.cr6.eq) goto loc_82B7B524;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[11].u32);
	// b 0x82b7b52c
	goto loc_82B7B52C;
loc_82B7B504:
	// extsb r11,r3
	ctx.r[11].s64 = ctx.r[3].s8;
	// cmpwi cr6,r11,10
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 10, ctx.xer);
	// stb r11,0(r25)
	PPC_STORE_U8(ctx.r[25].u32 + 0, ctx.r[11].u8);
	// addi r25,r25,1
	ctx.r[25].s64 = ctx.r[25].s64 + 1;
	// stw r25,88(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 88, ctx.r[25].u32);
	// beq cr6,0x82b7b524
	if (ctx.cr6.eq) goto loc_82B7B524;
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// b 0x82b7b4b0
	goto loc_82B7B4B0;
loc_82B7B524:
	// li r11,0
	ctx.r[11].s64 = 0;
	// stb r11,0(r25)
	PPC_STORE_U8(ctx.r[25].u32 + 0, ctx.r[11].u8);
loc_82B7B52C:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,176
	ctx.r[12].s64 = ctx.r[31].s64 + 176;
	// bl 0x82b7b564
	ctx.lr = 0x82B7B538;
	sub_82B7B564(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82B7B53C:
	// addi r1,r31,176
	ctx.r[1].s64 = ctx.r[31].s64 + 176;
	// b 0x82b7a728
	__restgprlr_24(ctx, base);
	return;
}

void Body_82B7B564(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-176
	ctx.r[31].s64 = ctx.r[12].s64 + -176;
	// std r30,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[30].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b7c8
	ctx.lr = 0x82B7B584;
	sub_82B7B7C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r30,-16(r1)
	ctx.r[30].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// lwz r12,-24(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -24);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82B7B708(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r31,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// addi r11,r11,19184
	ctx.r[11].s64 = ctx.r[11].s64 + 19184;
	// cmplw cr6,r31,r11
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x82b7b758
	if (ctx.cr6.lt) goto loc_82B7B758;
	// addi r10,r11,608
	ctx.r[10].s64 = ctx.r[11].s64 + 608;
	// cmplw cr6,r31,r10
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[10].u32, ctx.xer);
	// bgt cr6,0x82b7b758
	if (ctx.cr6.gt) goto loc_82B7B758;
	// subf r11,r11,r31
	ctx.r[11].s64 = ctx.r[31].s64 - ctx.r[11].s64;
	// srawi r11,r11,5
	ctx.xer.ca = std::uint8_t((ctx.r[11].s32 < 0)) & std::uint8_t(((ctx.r[11].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[11].s32 >> 5;
	// addi r3,r11,16
	ctx.r[3].s64 = ctx.r[11].s64 + 16;
	// bl 0x82b81b28
	ctx.lr = 0x82B7B748;
	sub_82B81B28(ctx, base);
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// ori r11,r11,32768
	ctx.r[11].u64 = ctx.r[11].u64 | 32768;
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// b 0x82b7b760
	goto loc_82B7B760;
loc_82B7B758:
	// addi r3,r31,32
	ctx.r[3].s64 = ctx.r[31].s64 + 32;
	// bl 0x830d9c6c
	ctx.lr = 0x82B7B760;
	__imp__RtlEnterCriticalSection(ctx, base);
loc_82B7B760:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r31,-16(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// blr
	return;
}

void Body_82B81648(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// bne cr6,0x82b8168c
	if (!ctx.cr6.eq) goto loc_82B8168C;
	// bl 0x82b7fd78
	ctx.lr = 0x82B81660;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,22
	ctx.r[10].s64 = 22;
	// li r7,0
	ctx.r[7].s64 = 0;
	// li r6,0
	ctx.r[6].s64 = 0;
	// li r5,0
	ctx.r[5].s64 = 0;
	// li r4,0
	ctx.r[4].s64 = 0;
	// li r3,0
	ctx.r[3].s64 = 0;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// bl 0x82b7fec0
	ctx.lr = 0x82B81684;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b81690
	goto loc_82B81690;
loc_82B8168C:
	// lwz r3,16(r3)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 16);
loc_82B81690:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
#undef PPC_LOAD_U8
#undef PPC_STORE_U8
#undef PPC_LOAD_U32
#undef PPC_STORE_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry!=0x82b7b2f8u)return false;
    Context ctx{};FromFull(ctx,state);Base base{memory,dependencies,state};
    Body_82B7B2F8(ctx,base);ToFull(state,ctx);return true;
}
}
