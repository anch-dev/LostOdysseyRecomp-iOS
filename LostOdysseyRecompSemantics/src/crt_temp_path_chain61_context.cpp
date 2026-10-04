#include "lo_semantics/crt_temp_path_chain61_context.h"
#include "lo_semantics/crt_numeric61_context.h"
#include "lo_semantics/crt_stream_scan_adjacent_context.h"
#include "lo_semantics/crt_stream_close_caller.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/detail/ppc_integer_context.h"
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::crt_temp_path_chain61_context
{
namespace
{
using namespace detail::ppc_integer_context;
struct Base { GuestMemory& memory; Dependencies dependencies; Registers& full; };
void Direct(GuestAddress entry,Context& c,Base& b)
{
    ToFull(b.full,c);const auto accepted=b.dependencies.accepted;
    bool applied=false;
    if(entry==0x823acbd0u)
    {
        auto raw=crt_context_adapter::ToRaw(b.full);
        applied=raw_allocation_context::Apply(entry,b.memory,b.dependencies.allocation,raw);
        if(applied) crt_context_adapter::FromRaw(b.full,raw);
    }
    else if(entry==0x8231b0d0u)
        applied=crt_numeric_parse_chain61_context::ApplySupport(entry,b.memory,accepted,b.full);
    else if(entry==0x82df3dc0u)
        applied=crt_numeric_parse_chain61_context::Apply(entry,b.memory,accepted,b.full);
    else if(entry==0x82df5fb0u)
        applied=crt_numeric61_context::Apply(entry,b.memory,accepted,b.full);
    else if(entry==0x82df5d18u)
        applied=crt_stream_scan_adjacent_context::Apply(entry,b.memory,accepted,b.full);
    else if(entry==0x82df4898u||entry==0x82df6908u)
        applied=crt_stream_close_shared_lower::Apply(entry,b.memory,accepted,b.full);
    else if(entry==0x82b7ff08u)
        applied=crt_stream_open_pipeline::ApplyLower(entry,b.memory,accepted.open.open,b.full);
    else if(entry==0x82b87b18u)
    {
        auto selected=crt_context_adapter::ToStream(b.full);
        applied=crt_stream_close_caller::Apply(entry,b.memory,accepted.close.pipeline.close,selected);
        if(applied) crt_context_adapter::FromStream(b.full,selected);
    }
    else applied=crt_stream_close_shared_lower::ApplyAcceptedLower(entry,b.memory,accepted,b.full);
    if(!applied) throw std::logic_error("missing accepted temporary-path lower");
    FromFull(c,b.full);
}
void sub_82B7FD78(Context& c,Base& b) { Direct(0x82b7fd78u,c,b); }
void sub_82B7FEC0(Context& c,Base& b) { Direct(0x82b7fec0u,c,b); }
void sub_82B819E8(Context& c,Base& b) { Direct(0x82b819e8u,c,b); }
void sub_82B81B28(Context& c,Base& b) { Direct(0x82b81b28u,c,b); }
void sub_8231B0D0(Context& c,Base& b) { Direct(0x8231b0d0u,c,b); }
void sub_82B7FF08(Context& c,Base& b) { Direct(0x82b7ff08u,c,b); }
void sub_82DF5FB0(Context& c,Base& b) { Direct(0x82df5fb0u,c,b); }
void sub_82DF5D18(Context& c,Base& b) { Direct(0x82df5d18u,c,b); }
void sub_82DF3DC0(Context& c,Base& b) { Direct(0x82df3dc0u,c,b); }
void sub_82DF4898(Context& c,Base& b) { Direct(0x82df4898u,c,b); }
void sub_82DF6908(Context& c,Base& b) { Direct(0x82df6908u,c,b); }
void sub_823ACBD0(Context& c,Base& b) { Direct(0x823acbd0u,c,b); }
void sub_82B87B18(Context& c,Base& b) { Direct(0x82b87b18u,c,b); }
void sub_82B7B7C8(Context& c,Base& b) { Direct(0x82b7b7c8u,c,b); }
void sub_82B819C8(Context& c,Base& b) { Direct(0x82b819c8u,c,b); }
void __savegprlr_25(Context& c,Base& b) { Save(25,c,b); }
void __restgprlr_25(Context& c,Base& b) { Restore(25,c,b); }
void __savegprlr_29(Context& c,Base& b) { Save(29,c,b); }
void __restgprlr_29(Context& c,Base& b) { Restore(29,c,b); }
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),(v))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),(v))
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),(v))
void Body_82290AA8(Context& ctx, Base& base) {
	// lwz r11,256(r13)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[13].u32 + 256);
	// lwz r3,332(r11)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 332);
	// blr
	return;
}
void sub_82290AA8(Context& c,Base& b) { Body_82290AA8(c,b); }
void Body_82DF4164(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-176
	ctx.r[31].s64 = ctx.r[12].s64 + -176;
	// std r29,-16(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -16, ctx.r[29].u64);
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-24(r1)
	PPC_STORE_U32(ctx.r[1].u32 + -24, ctx.r[12].u32);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// lwz r11,92(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 92);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq cr6,0x82df4190
	if (ctx.cr6.eq) goto loc_82DF4190;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// bl 0x82b7b7c8
	ctx.lr = 0x82DF4190;
	sub_82B7B7C8(ctx, base);
loc_82DF4190:
	// li r3,2
	ctx.r[3].s64 = 2;
	// bl 0x82b819c8
	ctx.lr = 0x82DF4198;
	sub_82B819C8(ctx, base);
	// lwz r1,0(r1)
	ctx.r[1].u64 = PPC_LOAD_U32(ctx.r[1].u32 + 0);
	// ld r31,-8(r1)
	ctx.r[31].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -8);
	// ld r29,-16(r1)
	ctx.r[29].u64 = PPC_LOAD_U64(ctx.r[1].u32 + -16);
	// lwz r12,-24(r1)
	ctx.r[12].u64 = PPC_LOAD_U32(ctx.r[1].u32 + -24);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// blr
	return;
}
void sub_82DF4164(Context& c,Base& b) { Body_82DF4164(c,b); }
void Body_82DF3770(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82DF3778;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82df37f8
	if (ctx.cr6.eq) goto loc_82DF37F8;
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
loc_82DF3790:
	// lbz r9,0(r11)
	ctx.r[9].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// bne cr6,0x82df3790
	if (!ctx.cr6.eq) goto loc_82DF3790;
	// subf r11,r10,r11
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[10].s64;
	// addi r11,r11,-1
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	// rotlwi r11,r11,0
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32, 0);
	// addi r30,r11,1
	ctx.r[30].s64 = ctx.r[11].s64 + 1;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x823acbd0
	ctx.lr = 0x82DF37B8;
	sub_823ACBD0(ctx, base);
	// mr. r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	ctx.cr0.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// beq 0x82df37f8
	if (ctx.cr0.eq) goto loc_82DF37F8;
	// mr r5,r29
	ctx.r[5].u64 = ctx.r[29].u64;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x8231b0d0
	ctx.lr = 0x82DF37D0;
	sub_8231B0D0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df37f0
	if (ctx.cr0.eq) goto loc_82DF37F0;
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
	// bl 0x82b7ff08
	ctx.lr = 0x82DF37F0;
	sub_82B7FF08(ctx, base);
loc_82DF37F0:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// b 0x82df37fc
	goto loc_82DF37FC;
loc_82DF37F8:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF37FC:
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}
void sub_82DF3770(Context& c,Base& b) { Body_82DF3770(c,b); }
void Body_82DF3E80(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x82DF3E88;
	__savegprlr_25(ctx, base);
	// addi r31,r1,-176
	ctx.r[31].s64 = ctx.r[1].s64 + -176;
	// stwu r1,-176(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-176);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r26,r3
	ctx.r[26].u64 = ctx.r[3].u64;
	// mr r25,r4
	ctx.r[25].u64 = ctx.r[4].u64;
	// cntlzw r11,r26
	ctx.r[11].u64 = ctx.r[26].u32 == 0 ? 32 : std::countl_zero(ctx.r[26].u32);
	// li r27,0
	ctx.r[27].s64 = 0;
	// stw r27,96(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 96, ctx.r[27].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// stw r27,92(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 92, ctx.r[27].u32);
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df3ee8
	if (!ctx.cr0.eq) goto loc_82DF3EE8;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF3EBC;
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
	ctx.lr = 0x82DF3EE0;
	sub_82B7FEC0(ctx, base);
	// li r3,22
	ctx.r[3].s64 = 22;
	// b 0x82df413c
	goto loc_82DF413C;
loc_82DF3EE8:
	// li r3,2
	ctx.r[3].s64 = 2;
	// stw r27,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[27].u32);
	// bl 0x82b819e8
	ctx.lr = 0x82DF3EF4;
	sub_82B819E8(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df3f08
	if (!ctx.cr0.eq) goto loc_82DF3F08;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF3F00;
	sub_82B7FD78(ctx, base);
	// lwz r3,0(r3)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// b 0x82df413c
	goto loc_82DF413C;
loc_82DF3F08:
	// li r3,2
	ctx.r[3].s64 = 2;
	// bl 0x82b81b28
	ctx.lr = 0x82DF3F10;
	sub_82B81B28(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lis r11,-31953
	ctx.r[11].s64 = -2094071808;
	// addi r30,r11,-13072
	ctx.r[30].s64 = ctx.r[11].s64 + -13072;
	// lbz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[30].u32 + 0);
	// cmplwi r11,0
	ctx.cr0.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// li r4,20
	ctx.r[4].s64 = 20;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bne 0x82df4010
	if (!ctx.cr0.eq) goto loc_82DF4010;
	// lis r11,-32235
	ctx.r[11].s64 = -2112552960;
	// addi r5,r11,32080
	ctx.r[5].s64 = ctx.r[11].s64 + 32080;
	// bl 0x8231b0d0
	ctx.lr = 0x82DF3F3C;
	sub_8231B0D0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df3f5c
	if (ctx.cr0.eq) goto loc_82DF3F5C;
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
	// bl 0x82b7ff08
	ctx.lr = 0x82DF3F5C;
	sub_82B7FF08(ctx, base);
loc_82DF3F5C:
	// lbz r11,2(r30)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[30].u32 + 2);
	// extsb r10,r11
	ctx.r[10].s64 = ctx.r[11].s8;
	// addi r11,r30,3
	ctx.r[11].s64 = ctx.r[30].s64 + 3;
	// stw r11,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[11].u32);
	// cmpwi cr6,r10,92
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 92, ctx.xer);
	// beq cr6,0x82df3f8c
	if (ctx.cr6.eq) goto loc_82DF3F8C;
	// cmpwi cr6,r10,47
	ctx.cr6.compare<int32_t>(ctx.r[10].s32, 47, ctx.xer);
	// beq cr6,0x82df3f8c
	if (ctx.cr6.eq) goto loc_82DF3F8C;
	// li r11,92
	ctx.r[11].s64 = 92;
	// stb r11,3(r30)
	PPC_STORE_U8(ctx.r[30].u32 + 3, ctx.r[11].u8);
	// addi r11,r30,4
	ctx.r[11].s64 = ctx.r[30].s64 + 4;
	// stw r11,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[11].u32);
loc_82DF3F8C:
	// li r10,116
	ctx.r[10].s64 = 116;
	// stb r10,0(r11)
	PPC_STORE_U8(ctx.r[11].u32 + 0, ctx.r[10].u8);
	// addi r29,r11,1
	ctx.r[29].s64 = ctx.r[11].s64 + 1;
	// stw r29,84(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 84, ctx.r[29].u32);
	// addi r11,r30,20
	ctx.r[11].s64 = ctx.r[30].s64 + 20;
	// subf r28,r29,r11
	ctx.r[28].s64 = ctx.r[11].s64 - ctx.r[29].s64;
	// bl 0x82290aa8
	ctx.lr = 0x82DF3FA8;
	sub_82290AA8(ctx, base);
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r5,r28
	ctx.r[5].u64 = ctx.r[28].u64;
	// li r6,32
	ctx.r[6].s64 = 32;
	// bl 0x82df5fb0
	ctx.lr = 0x82DF3FB8;
	sub_82DF5FB0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df3fd8
	if (ctx.cr0.eq) goto loc_82DF3FD8;
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
	// bl 0x82b7ff08
	ctx.lr = 0x82DF3FD8;
	sub_82B7FF08(ctx, base);
loc_82DF3FD8:
	// lis r11,-32223
	ctx.r[11].s64 = -2111766528;
	// addi r5,r11,31784
	ctx.r[5].s64 = ctx.r[11].s64 + 31784;
	// li r4,20
	ctx.r[4].s64 = 20;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df5d18
	ctx.lr = 0x82DF3FEC;
	sub_82DF5D18(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// beq 0x82df4024
	if (ctx.cr0.eq) goto loc_82DF4024;
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
	// bl 0x82b7ff08
	ctx.lr = 0x82DF400C;
	sub_82B7FF08(ctx, base);
	// b 0x82df4024
	goto loc_82DF4024;
loc_82DF4010:
	// lis r5,32767
	ctx.r[5].s64 = 2147418112;
	// ori r5,r5,65535
	ctx.r[5].u64 = ctx.r[5].u64 | 65535;
	// bl 0x82df3dc0
	ctx.lr = 0x82DF401C;
	sub_82DF3DC0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df4114
	if (!ctx.cr0.eq) goto loc_82DF4114;
loc_82DF4024:
	// bl 0x82df4898
	ctx.lr = 0x82DF4028;
	sub_82DF4898(ctx, base);
	// mr. r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	ctx.cr0.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// stw r29,88(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 88, ctx.r[29].u32);
	// bne 0x82df4040
	if (!ctx.cr0.eq) goto loc_82DF4040;
	// li r11,24
	ctx.r[11].s64 = 24;
	// stw r11,96(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 96, ctx.r[11].u32);
	// b 0x82df4118
	goto loc_82DF4118;
loc_82DF4040:
	// li r11,1
	ctx.r[11].s64 = 1;
	// stw r11,92(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 92, ctx.r[11].u32);
	// bl 0x82b7fd78
	ctx.lr = 0x82DF404C;
	sub_82B7FD78(ctx, base);
	// lwz r28,0(r3)
	ctx.r[28].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// bl 0x82b7fd78
	ctx.lr = 0x82DF4054;
	sub_82B7FD78(ctx, base);
	// stw r27,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[27].u32);
loc_82DF4058:
	// li r7,384
	ctx.r[7].s64 = 384;
	// mr r6,r25
	ctx.r[6].u64 = ctx.r[25].u64;
	// lis r5,0
	ctx.r[5].s64 = 0;
	// ori r5,r5,34114
	ctx.r[5].u64 = ctx.r[5].u64 | 34114;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// addi r3,r31,80
	ctx.r[3].s64 = ctx.r[31].s64 + 80;
	// bl 0x82df6908
	ctx.lr = 0x82DF4074;
	sub_82DF6908(ctx, base);
	// cmpwi cr6,r3,17
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, 17, ctx.xer);
	// bne cr6,0x82df40a0
	if (!ctx.cr6.eq) goto loc_82DF40A0;
	// lis r5,32767
	ctx.r[5].s64 = 2147418112;
	// ori r5,r5,65535
	ctx.r[5].u64 = ctx.r[5].u64 | 65535;
	// li r4,20
	ctx.r[4].s64 = 20;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df3dc0
	ctx.lr = 0x82DF4090;
	sub_82DF3DC0(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df40a0
	if (!ctx.cr0.eq) goto loc_82DF40A0;
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// b 0x82df4058
	goto loc_82DF4058;
loc_82DF40A0:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF40A4;
	sub_82B7FD78(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne cr6,0x82df40b8
	if (!ctx.cr6.eq) goto loc_82DF40B8;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF40B4;
	sub_82B7FD78(ctx, base);
	// stw r28,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[28].u32);
loc_82DF40B8:
	// lwz r11,80(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// cmpwi cr6,r11,-1
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, -1, ctx.xer);
	// beq cr6,0x82df4118
	if (ctx.cr6.eq) goto loc_82DF4118;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df3770
	ctx.lr = 0x82DF40CC;
	sub_82DF3770(ctx, base);
	// stw r3,28(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 28, ctx.r[3].u32);
	// rotlwi r11,r3,0
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32, 0);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// bne cr6,0x82df40e8
	if (!ctx.cr6.eq) goto loc_82DF40E8;
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// bl 0x82b87b18
	ctx.lr = 0x82DF40E4;
	sub_82B87B18(ctx, base);
	// b 0x82df4118
	goto loc_82DF4118;
loc_82DF40E8:
	// stw r27,4(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 4, ctx.r[27].u32);
	// stw r27,0(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 0, ctx.r[27].u32);
	// stw r27,8(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 8, ctx.r[27].u32);
	// lis r11,-31953
	ctx.r[11].s64 = -2094071808;
	// lwz r11,-13028(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -13028);
	// ori r11,r11,128
	ctx.r[11].u64 = ctx.r[11].u64 | 128;
	// stw r11,12(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 12, ctx.r[11].u32);
	// lwz r11,80(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// stw r11,16(r29)
	PPC_STORE_U32(ctx.r[29].u32 + 16, ctx.r[11].u32);
	// stw r29,0(r26)
	PPC_STORE_U32(ctx.r[26].u32 + 0, ctx.r[29].u32);
	// b 0x82df4118
	goto loc_82DF4118;
loc_82DF4114:
	// lwz r29,88(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 88);
loc_82DF4118:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,176
	ctx.r[12].s64 = ctx.r[31].s64 + 176;
	// bl 0x82df4164
	ctx.lr = 0x82DF4124;
	sub_82DF4164(ctx, base);
	// lwz r30,96(r31)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 96);
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// beq cr6,0x82df4138
	if (ctx.cr6.eq) goto loc_82DF4138;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF4134;
	sub_82B7FD78(ctx, base);
	// stw r30,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[30].u32);
loc_82DF4138:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
loc_82DF413C:
	// addi r1,r31,176
	ctx.r[1].s64 = ctx.r[31].s64 + 176;
	// b 0x82b7a72c
	__restgprlr_25(ctx, base);
	return;
}
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
} // namespace
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    if(entry!=0x82df3770u&&entry!=0x82df3e80u&&entry!=0x82df4164u) return false;
    Context c{};FromFull(c,state);Base b{memory,dependencies,state};
    if(entry==0x82df3770u) Body_82DF3770(c,b);
    else if(entry==0x82df3e80u) Body_82DF3E80(c,b);
    else Body_82DF4164(c,b);
    ToFull(state,c);return true;
}
} // namespace lo::semantic::gpu::crt_temp_path_chain61_context
