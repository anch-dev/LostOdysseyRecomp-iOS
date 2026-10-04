#include "lo_semantics/crt_close_block_output_context.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_format_stream.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
namespace lo::semantic::gpu::crt_close_block_output_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using std::int32_t;
using std::uint32_t;
using std::uint64_t;

// The authoritative generated bodies below retain their register operations
// and labels. This local context bridges those operations to the selected
// full register contract at every real direct or native call.
union PpcRegister
{
    uint64_t u64;
    std::int64_t s64;
    uint32_t u32;
    int32_t s32;
    std::int8_t s8;
    std::uint8_t u8;
    PpcRegister() : u64(0) {}
};
struct Xer { std::uint8_t so = 0, ca = 0; };
struct Cr
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    template<class T> void compare(T a, T b, const Xer& xer)
    {
        lt = std::uint8_t(a < b); gt = std::uint8_t(a > b);
        eq = std::uint8_t(a == b); so = xer.so;
    }
};
struct Context
{
    std::array<PpcRegister, 32> r{};
    uint64_t lr = 0, ctr = 0;
    Xer xer{};
    Cr cr0{}, cr6{};
};
struct Base
{
    GuestMemory& memory;
    Dependencies dependencies;
    Registers& full;
};
void FromFull(Context& ctx, const Registers& full)
{
    for (unsigned i = 0; i != 32; ++i) ctx.r[i].u64 = full.r[i];
    ctx.lr = full.lr; ctx.ctr = full.ctr;
    ctx.xer = {full.xer_so, full.xer_ca};
    ctx.cr0 = {full.cr0.lt, full.cr0.gt, full.cr0.eq, full.cr0.so};
    ctx.cr6 = {full.cr6.lt, full.cr6.gt, full.cr6.eq, full.cr6.so};
}
void ToFull(Registers& full, const Context& ctx)
{
    for (unsigned i = 0; i != 32; ++i) full.r[i] = ctx.r[i].u64;
    full.lr = ctx.lr; full.ctr = ctx.ctr;
    full.xer_so = ctx.xer.so; full.xer_ca = ctx.xer.ca;
    full.cr0 = {ctx.cr0.lt, ctx.cr0.gt, ctx.cr0.eq, ctx.cr0.so};
    full.cr6 = {ctx.cr6.lt, ctx.cr6.gt, ctx.cr6.eq, ctx.cr6.so};
}
void Save(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31; ++i)
        WriteU64(base.memory, Address(ctx.r[1].u64 - 16u - 8u * (31u - i)),
            ctx.r[i].u64);
    base.memory.WriteU32(Address(ctx.r[1].u64 - 8u), ctx.r[12].u32);
}
void Restore(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31; ++i)
        ctx.r[i].u64 = ReadU64(base.memory,
            Address(ctx.r[1].u64 - 16u - 8u * (31u - i)));
    ctx.r[12].u64 = base.memory.ReadU32(Address(ctx.r[1].u64 - 8u));
    ctx.lr = ctx.r[12].u64;
}

void __savegprlr_24(Context& c, Base& b) { Save(24,c,b); }
void __restgprlr_24(Context& c, Base& b) { Restore(24,c,b); }
void __savegprlr_27(Context& c, Base& b) { Save(27,c,b); }
void __restgprlr_27(Context& c, Base& b) { Restore(27,c,b); }
void Body_82DF2538(Context&,Base&);
void Body_82DF27B0(Context&,Base&);
void Body_82DF2884(Context&,Base&);

void Direct(GuestAddress entry,Context& ctx,Base& base)
{
    ToFull(base.full,ctx);
    if(entry==0x82df2538u) {Body_82DF2538(ctx,base);ToFull(base.full,ctx);}
    else if(entry==0x82df2884u) {Body_82DF2884(ctx,base);ToFull(base.full,ctx);}
    else if(entry==0x82b7a0b0u)
    {
        if(!crt_copy_full_context::Apply(entry,base.memory,base.full))
            throw std::logic_error("missing accepted full-context copy");
    }
    else if(entry==0x823add70u)
        base.dependencies.output.CallOutput(base.memory,base.full);
    else
    {
        auto lower=crt_context_adapter::ToStream(base.full);
        bool found=false;
        const auto& accepted=base.dependencies.accepted;
        if(entry==0x82b7b708u||entry==0x82b7b7c8u)
            found=crt_stream_bulk_close_routes::Apply(entry,base.memory,accepted.close,lower);
        else if(entry==0x82b7b838u)
            found=crt_format_stream::Apply(entry,base.memory,accepted.close.pipeline.format,lower);
        else
        {
            found=crt_stream_operations::Apply(entry,base.memory,accepted.close.pipeline.close.accepted,lower);
            if(!found)found=crt_stream_operations::ApplyAcceptedCallee(entry,base.memory,accepted.close.pipeline.close.accepted,lower);
        }
        if(!found)throw std::logic_error("missing accepted block-output callee");
        crt_context_adapter::FromStream(base.full,lower);
    }
    FromFull(ctx,base.full);
}
void sub_822A03C8(Context& c,Base& b) { Direct(0x822A03C8u,c,b); }
void sub_823ADD70(Context& c,Base& b) { Direct(0x823ADD70u,c,b); }
void sub_82B7A0B0(Context& c,Base& b) { Direct(0x82B7A0B0u,c,b); }
void sub_82B7B708(Context& c,Base& b) { Direct(0x82B7B708u,c,b); }
void sub_82B7B7C8(Context& c,Base& b) { Direct(0x82B7B7C8u,c,b); }
void sub_82B7B838(Context& c,Base& b) { Direct(0x82B7B838u,c,b); }
void sub_82B7FD78(Context& c,Base& b) { Direct(0x82B7FD78u,c,b); }
void sub_82B7FEC0(Context& c,Base& b) { Direct(0x82B7FEC0u,c,b); }
void sub_82B81648(Context& c,Base& b) { Direct(0x82B81648u,c,b); }
void sub_82B81DE0(Context& c,Base& b) { Direct(0x82B81DE0u,c,b); }
void sub_82B82558(Context& c,Base& b) { Direct(0x82B82558u,c,b); }
void sub_82DF2538(Context& c,Base& b) { Direct(0x82DF2538u,c,b); }
void sub_82DF2884(Context& c,Base& b) { Direct(0x82DF2884u,c,b); }

#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82DF2538(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6d8
	ctx.lr = 0x82DF2540;
	__savegprlr_24(ctx, base);
	// stwu r1,-416(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-416);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r25,r4
	ctx.r[25].u64 = ctx.r[4].u64;
	// mr r28,r3
	ctx.r[28].u64 = ctx.r[3].u64;
	// mr r24,r5
	ctx.r[24].u64 = ctx.r[5].u64;
	// mr r30,r6
	ctx.r[30].u64 = ctx.r[6].u64;
	// cmplwi cr6,r25,0
	ctx.cr6.compare<uint32_t>(ctx.r[25].u32, 0, ctx.xer);
	// beq cr6,0x82df2594
	if (ctx.cr6.eq) goto loc_82DF2594;
	// cmplwi cr6,r24,0
	ctx.cr6.compare<uint32_t>(ctx.r[24].u32, 0, ctx.xer);
	// beq cr6,0x82df2594
	if (ctx.cr6.eq) goto loc_82DF2594;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// bne cr6,0x82df25a0
	if (!ctx.cr6.eq) goto loc_82DF25A0;
loc_82DF256C:
	// bl 0x82b7fd78
	ctx.lr = 0x82DF2570;
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
	ctx.lr = 0x82DF2594;
	sub_82B7FEC0(ctx, base);
loc_82DF2594:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF2598:
	// addi r1,r1,416
	ctx.r[1].s64 = ctx.r[1].s64 + 416;
	// b 0x82b7a728
	__restgprlr_24(ctx, base);
	return;
loc_82DF25A0:
	// cmplwi cr6,r28,0
	ctx.cr6.compare<uint32_t>(ctx.r[28].u32, 0, ctx.xer);
	// beq cr6,0x82df256c
	if (ctx.cr6.eq) goto loc_82DF256C;
	// li r11,-1
	ctx.r[11].s64 = -1;
	// twllei r25,0
	// divwu r11,r11,r25
	ctx.r[11].u32 = ctx.r[11].u32 / ctx.r[25].u32;
	// cmplw cr6,r24,r11
	ctx.cr6.compare<uint32_t>(ctx.r[24].u32, ctx.r[11].u32, ctx.xer);
	// bgt cr6,0x82df256c
	if (ctx.cr6.gt) goto loc_82DF256C;
	// mullw r26,r25,r24
	ctx.r[26].s64 = int64_t(ctx.r[25].s32) * int64_t(ctx.r[24].s32);
	// mr r31,r26
	ctx.r[31].u64 = ctx.r[26].u64;
	// bl 0x822a03c8
	ctx.lr = 0x82DF25C8;
	sub_822A03C8(ctx, base);
	// addi r11,r3,32
	ctx.r[11].s64 = ctx.r[3].s64 + 32;
	// cmplw cr6,r30,r11
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[11].u32, ctx.xer);
	// beq cr6,0x82df2758
	if (ctx.cr6.eq) goto loc_82DF2758;
	// bl 0x822a03c8
	ctx.lr = 0x82DF25D8;
	sub_822A03C8(ctx, base);
	// addi r11,r3,64
	ctx.r[11].s64 = ctx.r[3].s64 + 64;
	// cmplw cr6,r30,r11
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[11].u32, ctx.xer);
	// beq cr6,0x82df2758
	if (ctx.cr6.eq) goto loc_82DF2758;
	// lwz r11,12(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 12);
	// andi. r11,r11,268
	ctx.r[11].u64 = ctx.r[11].u64 & 268;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df25fc
	if (ctx.cr0.eq) goto loc_82DF25FC;
	// lwz r27,24(r30)
	ctx.r[27].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 24);
	// b 0x82df2600
	goto loc_82DF2600;
loc_82DF25FC:
	// li r27,4096
	ctx.r[27].s64 = 4096;
loc_82DF2600:
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82df279c
	if (ctx.cr6.eq) goto loc_82DF279C;
loc_82DF2608:
	// lwz r11,12(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 12);
	// andi. r11,r11,264
	ctx.r[11].u64 = ctx.r[11].u64 & 264;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82df266c
	if (ctx.cr0.eq) goto loc_82DF266C;
	// lwz r29,4(r30)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 4);
	// cmpwi r29,0
	ctx.cr0.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// beq 0x82df266c
	if (ctx.cr0.eq) goto loc_82DF266C;
	// cmpwi cr6,r29,0
	ctx.cr6.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// blt cr6,0x82df26ec
	if (ctx.cr6.lt) goto loc_82DF26EC;
	// cmplw cr6,r31,r29
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[29].u32, ctx.xer);
	// bge cr6,0x82df2638
	if (!ctx.cr6.lt) goto loc_82DF2638;
	// mr r29,r31
	ctx.r[29].u64 = ctx.r[31].u64;
loc_82DF2638:
	// mr r5,r29
	ctx.r[5].u64 = ctx.r[29].u64;
	// lwz r3,0(r30)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// bl 0x82b7a0b0
	ctx.lr = 0x82DF2648;
	sub_82B7A0B0(ctx, base);
	// lwz r10,4(r30)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 4);
	// lwz r11,0(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 0);
	// subf r31,r29,r31
	ctx.r[31].s64 = ctx.r[31].s64 - ctx.r[29].s64;
	// subf r10,r29,r10
	ctx.r[10].s64 = ctx.r[10].s64 - ctx.r[29].s64;
	// add r11,r11,r29
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[29].u64;
	// add r28,r29,r28
	ctx.r[28].u64 = ctx.r[29].u64 + ctx.r[28].u64;
	// stw r10,4(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 4, ctx.r[10].u32);
	// stw r11,0(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 0, ctx.r[11].u32);
	// b 0x82df273c
	goto loc_82DF273C;
loc_82DF266C:
	// cmplw cr6,r31,r27
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, ctx.r[27].u32, ctx.xer);
	// blt cr6,0x82df2708
	if (ctx.cr6.lt) goto loc_82DF2708;
	// cmpwi cr6,r11,0
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq cr6,0x82df268c
	if (ctx.cr6.eq) goto loc_82DF268C;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b838
	ctx.lr = 0x82DF2684;
	sub_82B7B838(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// bne 0x82df2748
	if (!ctx.cr0.eq) goto loc_82DF2748;
loc_82DF268C:
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x82df26ac
	if (ctx.cr6.eq) goto loc_82DF26AC;
	// divwu r11,r31,r27
	ctx.r[11].u32 = ctx.r[31].u32 / ctx.r[27].u32;
	// twllei r27,0
	// mullw r11,r11,r27
	ctx.r[11].s64 = int64_t(ctx.r[11].s32) * int64_t(ctx.r[27].s32);
	// subf r11,r11,r31
	ctx.r[11].s64 = ctx.r[31].s64 - ctx.r[11].s64;
	// subf r29,r11,r31
	ctx.r[29].s64 = ctx.r[31].s64 - ctx.r[11].s64;
	// b 0x82df26b0
	goto loc_82DF26B0;
loc_82DF26AC:
	// mr r29,r31
	ctx.r[29].u64 = ctx.r[31].u64;
loc_82DF26B0:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b81648
	ctx.lr = 0x82DF26B8;
	sub_82B81648(ctx, base);
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// mr r5,r29
	ctx.r[5].u64 = ctx.r[29].u64;
	// bl 0x82b81de0
	ctx.lr = 0x82DF26C4;
	sub_82B81DE0(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df26ec
	if (ctx.cr6.eq) goto loc_82DF26EC;
	// cmplw cr6,r3,r29
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, ctx.r[29].u32, ctx.xer);
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
	// bgt cr6,0x82df26dc
	if (ctx.cr6.gt) goto loc_82DF26DC;
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
loc_82DF26DC:
	// subf r31,r11,r31
	ctx.r[31].s64 = ctx.r[31].s64 - ctx.r[11].s64;
	// add r28,r11,r28
	ctx.r[28].u64 = ctx.r[11].u64 + ctx.r[28].u64;
	// cmplw cr6,r3,r29
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, ctx.r[29].u32, ctx.xer);
	// bge cr6,0x82df273c
	if (!ctx.cr6.lt) goto loc_82DF273C;
loc_82DF26EC:
	// lwz r11,12(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 12);
	// subf r10,r31,r26
	ctx.r[10].s64 = ctx.r[26].s64 - ctx.r[31].s64;
	// twllei r25,0
	// ori r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 | 32;
	// divwu r3,r10,r25
	ctx.r[3].u32 = ctx.r[10].u32 / ctx.r[25].u32;
	// stw r11,12(r30)
	PPC_STORE_U32(ctx.r[30].u32 + 12, ctx.r[11].u32);
	// b 0x82df2598
	goto loc_82DF2598;
loc_82DF2708:
	// lbz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[28].u32 + 0);
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// extsb r3,r11
	ctx.r[3].s64 = ctx.r[11].s8;
	// bl 0x82b82558
	ctx.lr = 0x82DF2718;
	sub_82B82558(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82df2748
	if (ctx.cr6.eq) goto loc_82DF2748;
	// lwz r11,24(r30)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[30].u32 + 24);
	// addi r28,r28,1
	ctx.r[28].s64 = ctx.r[28].s64 + 1;
	// addi r31,r31,-1
	ctx.r[31].s64 = ctx.r[31].s64 + -1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// mr r27,r11
	ctx.r[27].u64 = ctx.r[11].u64;
	// bgt 0x82df273c
	if (ctx.cr0.gt) goto loc_82DF273C;
	// li r27,1
	ctx.r[27].s64 = 1;
loc_82DF273C:
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82df2608
	if (!ctx.cr6.eq) goto loc_82DF2608;
	// b 0x82df279c
	goto loc_82DF279C;
loc_82DF2748:
	// subf r11,r31,r26
	ctx.r[11].s64 = ctx.r[26].s64 - ctx.r[31].s64;
	// twllei r25,0
	// divwu r3,r11,r25
	ctx.r[3].u32 = ctx.r[11].u32 / ctx.r[25].u32;
	// b 0x82df2598
	goto loc_82DF2598;
loc_82DF2758:
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82df279c
	if (ctx.cr6.eq) goto loc_82DF279C;
loc_82DF2760:
	// cmplwi cr6,r31,255
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 255, ctx.xer);
	// mr r30,r31
	ctx.r[30].u64 = ctx.r[31].u64;
	// blt cr6,0x82df2770
	if (ctx.cr6.lt) goto loc_82DF2770;
	// li r30,255
	ctx.r[30].s64 = 255;
loc_82DF2770:
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// bl 0x82b7a0b0
	ctx.lr = 0x82DF2780;
	sub_82B7A0B0(ctx, base);
	// addi r11,r1,80
	ctx.r[11].s64 = ctx.r[1].s64 + 80;
	// li r10,0
	ctx.r[10].s64 = 0;
	// addi r3,r1,80
	ctx.r[3].s64 = ctx.r[1].s64 + 80;
	// stbx r10,r30,r11
	PPC_STORE_U8(ctx.r[30].u32 + ctx.r[11].u32, ctx.r[10].u8);
	// bl 0x823add70
	ctx.lr = 0x82DF2794;
	sub_823ADD70(ctx, base);
	// subf. r31,r30,r31
	ctx.r[31].s64 = ctx.r[31].s64 - ctx.r[30].s64;
	ctx.cr0.compare<int32_t>(ctx.r[31].s32, 0, ctx.xer);
	// bne 0x82df2760
	if (!ctx.cr0.eq) goto loc_82DF2760;
loc_82DF279C:
	// mr r3,r24
	ctx.r[3].u64 = ctx.r[24].u64;
	// b 0x82df2598
	goto loc_82DF2598;
}

void Body_82DF27B0(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x82DF27B8;
	__savegprlr_27(ctx, base);
	// addi r31,r1,-144
	ctx.r[31].s64 = ctx.r[1].s64 + -144;
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r27,r3
	ctx.r[27].u64 = ctx.r[3].u64;
	// mr r29,r4
	ctx.r[29].u64 = ctx.r[4].u64;
	// mr r28,r5
	ctx.r[28].u64 = ctx.r[5].u64;
	// mr r30,r6
	ctx.r[30].u64 = ctx.r[6].u64;
	// stw r30,188(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 188, ctx.r[30].u32);
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x82df2820
	if (ctx.cr6.eq) goto loc_82DF2820;
	// cmplwi cr6,r28,0
	ctx.cr6.compare<uint32_t>(ctx.r[28].u32, 0, ctx.xer);
	// beq cr6,0x82df2820
	if (ctx.cr6.eq) goto loc_82DF2820;
	// cntlzw r11,r30
	ctx.r[11].u64 = ctx.r[30].u32 == 0 ? 32 : std::countl_zero(ctx.r[30].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// xori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 ^ 1;
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82df282c
	if (!ctx.cr0.eq) goto loc_82DF282C;
	// bl 0x82b7fd78
	ctx.lr = 0x82DF27FC;
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
	ctx.lr = 0x82DF2820;
	sub_82B7FEC0(ctx, base);
loc_82DF2820:
	// li r3,0
	ctx.r[3].s64 = 0;
loc_82DF2824:
	// addi r1,r31,144
	ctx.r[1].s64 = ctx.r[31].s64 + 144;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
loc_82DF282C:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7b708
	ctx.lr = 0x82DF2834;
	sub_82B7B708(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// mr r6,r30
	ctx.r[6].u64 = ctx.r[30].u64;
	// mr r5,r28
	ctx.r[5].u64 = ctx.r[28].u64;
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r3,r27
	ctx.r[3].u64 = ctx.r[27].u64;
	// bl 0x82df2538
	ctx.lr = 0x82DF284C;
	sub_82DF2538(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,144
	ctx.r[12].s64 = ctx.r[31].s64 + 144;
	// bl 0x82df2884
	ctx.lr = 0x82DF285C;
	sub_82DF2884(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
	// b 0x82df2824
	goto loc_82DF2824;
}

void Body_82DF2884(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-144
	ctx.r[31].s64 = ctx.r[12].s64 + -144;
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
	ctx.lr = 0x82DF28A4;
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
#undef PPC_LOAD_U8
#undef PPC_LOAD_U32
#undef PPC_LOAD_U64
#undef PPC_STORE_U8
#undef PPC_STORE_U32
#undef PPC_STORE_U64
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    Context context{};FromFull(context,state);Base base{memory,dependencies,state};
    switch(entry)
    {
    case 0x82df2538u:Body_82DF2538(context,base);break;
    case 0x82df27b0u:Body_82DF27B0(context,base);break;
    case 0x82df2884u:Body_82DF2884(context,base);break;
    default:return false;
    }
    ToFull(state,context);return true;
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state)
{
    switch(entry)
    {
    case 0x822a03c8u:case 0x823add70u:case 0x82b7a0b0u:
    case 0x82b7b708u:case 0x82b7b7c8u:case 0x82b7b838u:
    case 0x82b7fd78u:case 0x82b7fec0u:case 0x82b81648u:
    case 0x82b81de0u:case 0x82b82558u:break;
    default:return false;
    }
    Context context{};FromFull(context,state);Base base{memory,dependencies,state};
    Direct(entry,context,base);return true;
}
} // namespace lo::semantic::gpu::crt_close_block_output_context
