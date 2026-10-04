#include "lo_semantics/crt_close_reader_callers_context.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_close_reader_callers_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using std::int32_t;
using std::uint32_t;
using std::uint64_t;
union PpcRegister
{
    uint64_t u64;
    std::int64_t s64;
    uint32_t u32;
    int32_t s32;
    std::uint16_t u16;
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
    for (unsigned i = 0; i < 32u; ++i) ctx.r[i].u64 = full.r[i];
    ctx.lr = full.lr; ctx.ctr = full.ctr;
    ctx.xer = {full.xer_so, full.xer_ca};
    ctx.cr0 = {full.cr0.lt, full.cr0.gt, full.cr0.eq, full.cr0.so};
    ctx.cr6 = {full.cr6.lt, full.cr6.gt, full.cr6.eq, full.cr6.so};
}
void ToFull(Registers& full, const Context& ctx)
{
    for (unsigned i = 0; i < 32u; ++i) full.r[i] = ctx.r[i].u64;
    full.lr = ctx.lr; full.ctr = ctx.ctr;
    full.xer_so = ctx.xer.so; full.xer_ca = ctx.xer.ca;
    full.cr0 = {ctx.cr0.lt, ctx.cr0.gt, ctx.cr0.eq, ctx.cr0.so};
    full.cr6 = {ctx.cr6.lt, ctx.cr6.gt, ctx.cr6.eq, ctx.cr6.so};
}
void Save(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(base.memory, Address(ctx.r[1].u64 - 8u * (33u - i)),
            ctx.r[i].u64);
    base.memory.WriteU32(Address(ctx.r[1].u64 - 8u), ctx.r[12].u32);
}
void Restore(unsigned first, Context& ctx, Base& base)
{
    for (unsigned i = first; i <= 31u; ++i)
        ctx.r[i].u64 = ReadU64(base.memory,
            Address(ctx.r[1].u64 - 8u * (33u - i)));
    ctx.r[12].u64 = base.memory.ReadU32(Address(ctx.r[1].u64 - 8u));
    ctx.lr = ctx.r[12].u64;
}
void __savegprlr_25(Context& c, Base& b) { Save(25u,c,b); }
void __restgprlr_25(Context& c, Base& b) { Restore(25u,c,b); }
void __savegprlr_27(Context& c, Base& b) { Save(27u,c,b); }
void __restgprlr_27(Context& c, Base& b) { Restore(27u,c,b); }
void Body_82BD0A60(Context&, Base&);
void Body_82BD0D28(Context&, Base&);
void Indirect(Context& ctx, Base& base)
{
    ToFull(base.full,ctx);
    base.dependencies.guest.CallIndirect(Address(ctx.ctr) & ~3u,
        base.memory,base.full);
    FromFull(ctx,base.full);
}
void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full,ctx);
    if(entry == 0x82bd0a60u)
    {
        Body_82BD0A60(ctx,base);
        ToFull(base.full,ctx);
    }
    else if(entry == 0x82bd0798u)
    {
        if(!crt_close_recursive_buffer_context::Apply(entry,base.memory,
                base.dependencies.guest,base.full))
            throw std::logic_error("missing accepted lazy table");
    }
    else if(entry == 0x82df2520u)
    {
        if(!crt_stream_block_read_context::Apply(entry,base.memory,
                base.dependencies.accepted,base.full))
            throw std::logic_error("missing selected block read");
    }
    else if(entry == 0x82df1ea8u)
    {
        if(!crt_stream_close_reopen_context::Apply(entry,base.memory,
                base.dependencies.accepted,base.full))
            throw std::logic_error("missing accepted reopen lower");
    }
    else if(entry == 0x82df1cd0u)
    {
        if(!crt_stream_close_file_lower::Apply(entry,base.memory,
                base.dependencies.accepted,base.full))
            throw std::logic_error("missing accepted close-file lower");
    }
    else if(entry == 0x82df2150u)
    {
        if(!crt_stream_close_shared_lower::Apply(entry,base.memory,
                base.dependencies.accepted,base.full))
            throw std::logic_error("missing accepted shared close lower");
    }
    else if(entry == 0x82b7a0b0u)
    {
        if(!crt_copy_full_context::Apply(entry,base.memory,base.full))
            throw std::logic_error("missing accepted full copy");
    }
    else
    {
        auto lower=crt_context_adapter::ToStream(base.full);
        if(entry != 0x82b85d88u ||
            !crt_stream_bulk_close_routes::Apply(entry,base.memory,
                base.dependencies.accepted.close,lower))
            throw std::logic_error("missing accepted bulk close");
        crt_context_adapter::FromStream(base.full,lower);
    }
    FromFull(ctx,base.full);
}
void sub_82BD0798(Context& c, Base& b) { Direct(0x82bd0798u,c,b); }
void sub_82DF2520(Context& c, Base& b) { Direct(0x82df2520u,c,b); }
void sub_82B7A0B0(Context& c, Base& b) { Direct(0x82b7a0b0u,c,b); }
void sub_82DF2150(Context& c, Base& b) { Direct(0x82df2150u,c,b); }
void sub_82DF1EA8(Context& c, Base& b) { Direct(0x82df1ea8u,c,b); }
void sub_82DF1CD0(Context& c, Base& b) { Direct(0x82df1cd0u,c,b); }
void sub_82B85D88(Context& c, Base& b) { Direct(0x82b85d88u,c,b); }
void sub_82BD0A60(Context& c, Base& b) { Direct(0x82bd0a60u,c,b); }
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U16(a,v) base.memory.WriteU16(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
void Body_82BD0A60(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x82BD0A68;
	__savegprlr_25(ctx, base);
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r28,r3
	ctx.r[28].u64 = ctx.r[3].u64;
	// mr r29,r4
	ctx.r[29].u64 = ctx.r[4].u64;
	// mr r27,r5
	ctx.r[27].u64 = ctx.r[5].u64;
	// mr r26,r6
	ctx.r[26].u64 = ctx.r[6].u64;
	// mr r25,r7
	ctx.r[25].u64 = ctx.r[7].u64;
	// bl 0x82bd0798
	ctx.lr = 0x82BD0A84;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// li r5,36
	ctx.r[5].s64 = 36;
	// li r4,16
	ctx.r[4].s64 = 16;
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// mtctr r11
	ctx.ctr = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0A9C;
	Indirect(ctx, base);
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq cr6,0x82bd0ae8
	if (ctx.cr6.eq) goto loc_82BD0AE8;
	// li r30,0
	ctx.r[30].s64 = 0;
	// stw r30,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[30].u32);
	// stw r30,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[30].u32);
	// stw r29,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[29].u32);
	// bl 0x82bd0798
	ctx.lr = 0x82BD0ABC;
	sub_82BD0798(ctx, base);
	// lwz r11,0(r3)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[3].u32 + 0);
	// li r5,65
	ctx.r[5].s64 = 65;
	// lwz r4,8(r31)
	ctx.r[4].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// mtctr r11
	ctx.ctr = ctx.r[11].u64;
	// bctrl
	ctx.lr = 0x82BD0AD4;
	Indirect(ctx, base);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// stw r3,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[3].u32);
	// beq cr6,0x82bd0ae8
	if (ctx.cr6.eq) goto loc_82BD0AE8;
	// stw r30,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[30].u32);
	// stw r31,0(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 0, ctx.r[31].u32);
loc_82BD0AE8:
	// lwz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// stw r11,4(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 4, ctx.r[11].u32);
	// bne cr6,0x82bd0b30
	if (!ctx.cr6.eq) goto loc_82BD0B30;
	// cmplwi cr6,r26,0
	ctx.cr6.compare<uint32_t>(ctx.r[26].u32, 0, ctx.xer);
	// beq cr6,0x82bd0b1c
	if (ctx.cr6.eq) goto loc_82BD0B1C;
	// mr r6,r26
	ctx.r[6].u64 = ctx.r[26].u64;
	// lwz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// li r5,1
	ctx.r[5].s64 = 1;
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// bl 0x82df2520
	ctx.lr = 0x82BD0B14;
	sub_82DF2520(ctx, base);
loc_82BD0B14:
	// lwz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// stw r25,4(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 4, ctx.r[25].u32);
loc_82BD0B1C:
	// lwz r11,0(r28)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[28].u32 + 0);
	// lwz r11,0(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// stw r11,16(r28)
	PPC_STORE_U32(ctx.r[28].u32 + 16, ctx.r[11].u32);
	// addi r1,r1,144
	ctx.r[1].s64 = ctx.r[1].s64 + 144;
	// b 0x82b7a72c
	__restgprlr_25(ctx, base);
	return;
loc_82BD0B30:
	// mr r5,r29
	ctx.r[5].u64 = ctx.r[29].u64;
	// lwz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[11].u32 + 0);
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// bl 0x82b7a0b0
	ctx.lr = 0x82BD0B40;
	sub_82B7A0B0(ctx, base);
	// b 0x82bd0b14
	goto loc_82BD0B14;
}

void Body_82BD0D28(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x82BD0D30;
	__savegprlr_27(ctx, base);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// li r11,0
	ctx.r[11].s64 = 0;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r29,r4
	ctx.r[29].u64 = ctx.r[4].u64;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// stw r11,8(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 8, ctx.r[11].u32);
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// sth r11,20(r31)
	PPC_STORE_U16(ctx.r[31].u32 + 20, ctx.r[11].u16);
	// sth r11,22(r31)
	PPC_STORE_U16(ctx.r[31].u32 + 22, ctx.r[11].u16);
	// stb r11,24(r31)
	PPC_STORE_U8(ctx.r[31].u32 + 24, ctx.r[11].u8);
	// stb r11,25(r31)
	PPC_STORE_U8(ctx.r[31].u32 + 25, ctx.r[11].u8);
	// lis r11,-32243
	ctx.r[11].s64 = -2113077248;
	// addi r27,r11,24808
	ctx.r[27].s64 = ctx.r[11].s64 + 24808;
	// beq cr6,0x82bd0da8
	if (ctx.cr6.eq) goto loc_82BD0DA8;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// bl 0x82df2150
	ctx.lr = 0x82BD0D74;
	sub_82DF2150(ctx, base);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x82bd0da8
	if (ctx.cr6.eq) goto loc_82BD0DA8;
	// li r5,2
	ctx.r[5].s64 = 2;
	// li r4,0
	ctx.r[4].s64 = 0;
	// bl 0x82df1ea8
	ctx.lr = 0x82BD0D8C;
	sub_82DF1EA8(ctx, base);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82df1cd0
	ctx.lr = 0x82BD0D94;
	sub_82DF1CD0(ctx, base);
	// mr r28,r3
	ctx.r[28].u64 = ctx.r[3].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b85d88
	ctx.lr = 0x82BD0DA0;
	sub_82B85D88(ctx, base);
	// cmplwi cr6,r28,0
	ctx.cr6.compare<uint32_t>(ctx.r[28].u32, 0, ctx.xer);
	// bne cr6,0x82bd0dac
	if (!ctx.cr6.eq) goto loc_82BD0DAC;
loc_82BD0DA8:
	// li r28,4096
	ctx.r[28].s64 = 4096;
loc_82BD0DAC:
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// bl 0x82df2150
	ctx.lr = 0x82BD0DB8;
	sub_82DF2150(ctx, base);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x82bd0de8
	if (ctx.cr6.eq) goto loc_82BD0DE8;
	// li r7,0
	ctx.r[7].s64 = 0;
	// mr r6,r30
	ctx.r[6].u64 = ctx.r[30].u64;
	// li r5,0
	ctx.r[5].s64 = 0;
	// mr r4,r28
	ctx.r[4].u64 = ctx.r[28].u64;
	// bl 0x82bd0a60
	ctx.lr = 0x82BD0DDC;
	sub_82BD0A60(ctx, base);
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b85d88
	ctx.lr = 0x82BD0DE4;
	sub_82B85D88(ctx, base);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
loc_82BD0DE8:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a734
	__restgprlr_27(ctx, base);
	return;
}
#undef PPC_STORE_U32
#undef PPC_STORE_U16
#undef PPC_STORE_U8
#undef PPC_LOAD_U32
} // namespace
bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if(entry != 0x82bd0a60u && entry != 0x82bd0d28u) return false;
    Context ctx{};
    FromFull(ctx,state);
    Base base{memory,dependencies,state};
    if(entry == 0x82bd0a60u) Body_82BD0A60(ctx,base);
    else Body_82BD0D28(ctx,base);
    ToFull(state,ctx);
    return true;
}
} // namespace lo::semantic::gpu::crt_close_reader_callers_context
