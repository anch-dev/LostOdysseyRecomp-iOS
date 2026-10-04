#include "lo_semantics/crt_flush_full61.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <stdexcept>
namespace lo::semantic::gpu::crt_flush_full61 {
namespace {
using namespace detail::ppc_integer_context;
struct Base {GuestMemory& memory;Dependencies dependencies;Registers& full;};
void Direct(GuestAddress entry,Context& ctx,Base& base) {
    ToFull(base.full,ctx);
    if(!ApplyAcceptedLower(entry,base.memory,base.dependencies,base.full))
        throw std::logic_error("missing accepted full flush lower");
    FromFull(ctx,base.full);
}
void __imp__RtlEnterCriticalSection(Context& ctx,Base& base){ToFull(base.full,ctx);base.dependencies.native.EnterCriticalSection(base.memory,base.full);FromFull(ctx,base.full);}
void __imp__RtlLeaveCriticalSection(Context& ctx,Base& base){ToFull(base.full,ctx);base.dependencies.native.LeaveCriticalSection(base.memory,base.full);FromFull(ctx,base.full);}
void __imp__NtFlushBuffersFile(Context& ctx,Base& base){ToFull(base.full,ctx);base.dependencies.native.NtFlushBuffersFile(base.memory,base.full);FromFull(ctx,base.full);}
void __imp__RtlNtStatusToDosError(Context& ctx,Base& base){ToFull(base.full,ctx);base.dependencies.native.RtlNtStatusToDosError(base.memory,base.full);FromFull(ctx,base.full);}
void __savegprlr_27(Context& c,Base& b){Save(27,c,b);}
void __restgprlr_27(Context& c,Base& b){Restore(27,c,b);}
void __savegprlr_29(Context& c,Base& b){Save(29,c,b);}
void __restgprlr_29(Context& c,Base& b){Restore(29,c,b);}
void Body_82B81F78(Context&,Base&);
void Body_82BE4898(Context&,Base&);
void sub_82BE4898(Context& c,Base& b){Body_82BE4898(c,b);}
void Body_82B820C4(Context&,Base&);
void sub_82B820C4(Context& c,Base& b){Body_82B820C4(c,b);}
void Body_827CA628(Context&,Base&);
void sub_827CA628(Context& c,Base& b){Body_827CA628(c,b);}
void Body_82B863F0(Context&,Base&);
void sub_82B863F0(Context& c,Base& b){Body_82B863F0(c,b);}
void Body_82B862F8(Context&,Base&);
void sub_82B862F8(Context& c,Base& b){Body_82B862F8(c,b);}
void Body_82B863B8(Context&,Base&);
void sub_82B863B8(Context& c,Base& b){Body_82B863B8(c,b);}
void sub_82B7FD78(Context& c,Base& b){Direct(0x82b7fd78u,c,b);}
void sub_82B7FEC0(Context& c,Base& b){Direct(0x82b7fec0u,c,b);}
void sub_82B7FDB0(Context& c,Base& b){Direct(0x82b7fdb0u,c,b);}
void sub_822CA100(Context& c,Base& b){Direct(0x822ca100u,c,b);}
void sub_82B86228(Context& c,Base& b){Direct(0x82b86228u,c,b);}
void sub_82B81B28(Context& c,Base& b){Direct(0x82b81b28u,c,b);}
void sub_82B821B0(Context& c,Base& b){Direct(0x82b821b0u,c,b);}
void sub_82B819C8(Context& c,Base& b){Direct(0x82b819c8u,c,b);}
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82B81F78(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x82B81F80;
	__savegprlr_27(ctx, base);
	// addi r31,r1,-144
	ctx.r[31].s64 = ctx.r[1].s64 + -144;
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r27,r3
	ctx.r[27].u64 = ctx.r[3].u64;
	// stw r27,164(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 164, ctx.r[27].u32);
	// cmpwi cr6,r27,-2
	ctx.cr6.compare<int32_t>(ctx.r[27].s32, -2, ctx.xer);
	// bne cr6,0x82b81fb0
	if (!ctx.cr6.eq) goto loc_82B81FB0;
	// bl 0x82b7fd78
	ctx.lr = 0x82B81F9C;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,9
	ctx.r[10].s64 = 9;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// b 0x82b8209c
	goto loc_82B8209C;
loc_82B81FB0:
	// cmpwi cr6,r27,0
	ctx.cr6.compare<int32_t>(ctx.r[27].s32, 0, ctx.xer);
	// blt cr6,0x82b81fc8
	if (ctx.cr6.lt) goto loc_82B81FC8;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// lwz r11,-29336(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -29336);
	// cmplw cr6,r27,r11
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x82b81ff8
	if (ctx.cr6.lt) goto loc_82B81FF8;
loc_82B81FC8:
	// bl 0x82b7fd78
	ctx.lr = 0x82B81FCC;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,9
	ctx.r[10].s64 = 9;
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
	ctx.lr = 0x82B81FF0;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b8209c
	goto loc_82B8209C;
loc_82B81FF8:
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29312
	ctx.r[30].s64 = ctx.r[11].s64 + -29312;
	// srawi r11,r27,5
	ctx.xer.ca = std::uint8_t((ctx.r[27].s32 < 0)) & std::uint8_t(((ctx.r[27].u32 & 0x1F) != 0));
	ctx.r[11].s64 = ctx.r[27].s32 >> 5;
	// rlwinm r28,r11,2,0,29
	ctx.r[28].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r29,r27,6,21,25
	ctx.r[29].u64 = std::rotl(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 6) & 0x7C0;
	// lwzx r11,r28,r30
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[30].u32);
	// add r11,r29,r11
	ctx.r[11].u64 = ctx.r[29].u64 + ctx.r[11].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b81fc8
	if (ctx.cr0.eq) goto loc_82B81FC8;
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82b862f8
	ctx.lr = 0x82B82028;
	sub_82B862F8(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwzx r11,r28,r30
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + ctx.r[30].u32);
	// add r11,r29,r11
	ctx.r[11].u64 = ctx.r[29].u64 + ctx.r[11].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b82078
	if (ctx.cr0.eq) goto loc_82B82078;
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82b86228
	ctx.lr = 0x82B82048;
	sub_82B86228(ctx, base);
	// bl 0x82be4898
	ctx.lr = 0x82B8204C;
	sub_82BE4898(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82b82060
	if (!ctx.cr0.eq) goto loc_82B82060;
	// bl 0x822ca100
	ctx.lr = 0x82B82058;
	sub_822CA100(ctx, base);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// b 0x82b82064
	goto loc_82B82064;
loc_82B82060:
	// li r30,0
	ctx.r[30].s64 = 0;
loc_82B82064:
	// stw r30,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[30].u32);
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// beq cr6,0x82b8208c
	if (ctx.cr6.eq) goto loc_82B8208C;
	// bl 0x82b7fdb0
	ctx.lr = 0x82B82074;
	sub_82B7FDB0(ctx, base);
	// stw r30,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[30].u32);
loc_82B82078:
	// bl 0x82b7fd78
	ctx.lr = 0x82B8207C;
	sub_82B7FD78(ctx, base);
	// li r11,9
	ctx.r[11].s64 = 9;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// li r11,-1
	ctx.r[11].s64 = -1;
	// stw r11,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[11].u32);
loc_82B8208C:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,144
	ctx.r[12].s64 = ctx.r[31].s64 + 144;
	// bl 0x82b820c4
	ctx.lr = 0x82B82098;
	sub_82B820C4(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82B8209C:
	// addi r1,r31,144
	ctx.r[1].s64 = ctx.r[31].s64 + 144;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
}

void Body_82BE4898(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// addi r4,r1,80
	ctx.r[4].s64 = ctx.r[1].s64 + 80;
	// bl 0x830da39c
	ctx.lr = 0x82BE48AC;
	__imp__NtFlushBuffersFile(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// blt 0x82be48bc
	if (ctx.cr0.lt) goto loc_82BE48BC;
	// li r3,1
	ctx.r[3].s64 = 1;
	// b 0x82be48c4
	goto loc_82BE48C4;
loc_82BE48BC:
	// bl 0x827ca628
	ctx.lr = 0x82BE48C0;
	sub_827CA628(ctx, base);
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82BE48C4:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82B820C4(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-144
	ctx.r[31].s64 = ctx.r[12].s64 + -144;
	// std r27,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[27].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82b863f0
	ctx.lr = 0x82B820E4;
	sub_82B863F0(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r27,-16(r1)
	ctx.r[27].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// lwz r12,-24(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -24);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_827CA628(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// bl 0x830d9efc
	ctx.lr = 0x827CA638;
	__imp__RtlNtStatusToDosError(ctx, base);
	// lwz r11,336(r13)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[13].u32 + 336);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x827ca64c
	if (!ctx.cr6.eq) goto loc_827CA64C;
	// lwz r11,256(r13)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[13].u32 + 256);
	// stw r3,352(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 352, ctx.r[3].u32);
loc_827CA64C:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}

void Body_82B863F0(Context& ctx, Base& base) {
	// srawi r10,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[10].s64 = ctx.r[3].s32 >> 5;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// rlwinm r9,r10,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// addi r11,r11,-29312
	ctx.r[11].s64 = ctx.r[11].s64 + -29312;
	// rlwinm r10,r3,6,21,25
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// lwzx r11,r9,r11
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[9].u32 + ctx.r[11].u32);
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// addi r3,r11,12
	ctx.r[3].s64 = ctx.r[11].s64 + 12;
	// b 0x830d9c7c
	__imp__RtlLeaveCriticalSection(ctx, base);
	return;
}

void Body_82B862F8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82B86300;
	__savegprlr_29(ctx, base);
	// addi r31,r1,-128
	ctx.r[31].s64 = ctx.r[1].s64 + -128;
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// stw r3,148(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 148, ctx.r[3].u32);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r11,r11,-29312
	ctx.r[11].s64 = ctx.r[11].s64 + -29312;
	// srawi r10,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[10].s64 = ctx.r[3].s32 >> 5;
	// rlwinm r10,r10,2,0,29
	ctx.r[10].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r9,r3,6,21,25
	ctx.r[9].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// li r29,1
	ctx.r[29].s64 = 1;
	// stw r29,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[29].u32);
	// lwzx r10,r10,r11
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[10].u32 + ctx.r[11].u32);
	// add r30,r10,r9
	ctx.r[30].u64 = ctx.r[10].u64 + ctx.r[9].u64;
	// lwz r10,8(r30)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 8);
	// cmpwi cr6,r10,0
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne cr6,0x82b86388
	if (!ctx.cr6.eq) goto loc_82B86388;
	// li r3,10
	ctx.r[3].s64 = 10;
	// bl 0x82b81b28
	ctx.lr = 0x82B86344;
	sub_82B81B28(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwz r11,8(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 8);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82b8637c
	if (!ctx.cr6.eq) goto loc_82B8637C;
	// li r4,4000
	ctx.r[4].s64 = 4000;
	// addi r3,r30,12
	ctx.r[3].s64 = ctx.r[30].s64 + 12;
	// bl 0x82b821b0
	ctx.lr = 0x82B86360;
	sub_82B821B0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82b86370
	if (!ctx.cr0.eq) goto loc_82B86370;
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[11].u32);
loc_82B86370:
	// lwz r11,8(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 8);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// stw r11,8(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 8, ctx.r[11].u32);
loc_82B8637C:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,128
	ctx.r[12].s64 = ctx.r[31].s64 + 128;
	// bl 0x82b863b8
	ctx.lr = 0x82B86388;
	sub_82B863B8(ctx, base);
loc_82B86388:
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// beq cr6,0x82b863ac
	if (ctx.cr6.eq) goto loc_82B863AC;
	// srawi r10,r3,5
	ctx.xer.ca = std::uint8_t((ctx.r[3].s32 < 0)) & std::uint8_t(((ctx.r[3].u32 & 0x1F) != 0));
	ctx.r[10].s64 = ctx.r[3].s32 >> 5;
	// rlwinm r9,r10,2,0,29
	ctx.r[9].u64 = std::rotl(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r10,r3,6,21,25
	ctx.r[10].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// lwzx r11,r9,r11
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[9].u32 + ctx.r[11].u32);
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// addi r3,r11,12
	ctx.r[3].s64 = ctx.r[11].s64 + 12;
	// bl 0x830d9c6c
	ctx.lr = 0x82B863AC;
	__imp__RtlEnterCriticalSection(ctx, base);
loc_82B863AC:
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// addi r1,r31,128
	ctx.r[1].s64 = ctx.r[31].s64 + 128;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}

void Body_82B863B8(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -8, ctx.r[12].u32);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r3,10
	ctx.r[3].s64 = 10;
	// bl 0x82b819c8
	ctx.lr = 0x82B863CC;
	sub_82B819C8(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r11,r11,-29312
	ctx.r[11].s64 = ctx.r[11].s64 + -29312;
	// lwz r3,148(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 148);
	// lwz r29,80(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
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
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry==0x82b86228u||entry==0x822ca100u||entry==0x82b7fdb0u) {
        auto lower=crt_context_adapter::ToStream(state);
        if(!crt_stream_operations::ApplyAcceptedCallee(entry,memory,
            dependencies.accepted.open.open.stream,lower))return false;
        crt_context_adapter::FromStream(state,lower);return true;
    }
    return crt_stream_close_shared_lower::ApplyAcceptedLower(entry,memory,dependencies.accepted,state);
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state) {
    if(entry!=0x82b81f78u)return false;
    Context ctx{};FromFull(ctx,state);Base base{memory,dependencies,state};
    Body_82B81F78(ctx,base);ToFull(state,ctx);return true;
}
}
