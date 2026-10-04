#include "lo_semantics/crt_stream_byte_read_context.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_byte_read_context
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
std::int32_t ArithmeticShiftRight32(std::int32_t value, unsigned bits)
{
    const auto raw = std::bit_cast<std::uint32_t>(value);
    const auto fill = value < 0 ? ~(UINT32_MAX >> bits) : 0u;
    return std::bit_cast<std::int32_t>((raw >> bits) | fill);
}
void Save29(Context& ctx, Base& base)
{
    for (unsigned i = 29u; i <= 31u; ++i)
        WriteU64(base.memory, Address(ctx.r[1].u64 - 8u * (33u - i)),
            ctx.r[i].u64);
    base.memory.WriteU32(Address(ctx.r[1].u64 - 8u), ctx.r[12].u32);
}
void Restore29(Context& ctx, Base& base)
{
    for (unsigned i = 29u; i <= 31u; ++i)
        ctx.r[i].u64 = ReadU64(base.memory,
            Address(ctx.r[1].u64 - 8u * (33u - i)));
    ctx.r[12].u64 = base.memory.ReadU32(Address(ctx.r[1].u64 - 8u));
    ctx.lr = ctx.r[12].u64;
}
void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    if (entry == 0x82b85a80u)
    {
        if (!crt_stream_refill_context::Apply(entry, base.memory,
                base.dependencies, base.full))
            throw std::logic_error("missing selected refill lower");
    }
    else if (entry == 0x82b7fd78u || entry == 0x82b7fec0u)
    {
        if (!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
                base.memory, base.dependencies, base.full))
            throw std::logic_error("missing accepted stream error lower");
    }
    else
    {
        auto lower = crt_context_adapter::ToStream(base.full);
        if (!crt_stream_operations::ApplyAcceptedCallee(entry, base.memory,
                base.dependencies.close.pipeline.close.accepted, lower))
            throw std::logic_error("missing accepted stream state lower");
        crt_context_adapter::FromStream(base.full, lower);
    }
    FromFull(ctx, base.full);
}
void sub_82B7FD78(Context& c, Base& b) { Direct(0x82b7fd78u,c,b); }
void sub_82B7FEC0(Context& c, Base& b) { Direct(0x82b7fec0u,c,b); }
void sub_82B85C40(Context& c, Base& b) { Direct(0x82b85c40u,c,b); }
void sub_82B81648(Context& c, Base& b) { Direct(0x82b81648u,c,b); }
void sub_82B85A80(Context& c, Base& b) { Direct(0x82b85a80u,c,b); }
#define __savegprlr_29(c,b) Save29(c,b)
#define __restgprlr_29(c,b) Restore29(c,b)
#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
void Body(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x82B81368;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// cmplwi cr6,r31,0
	ctx.cr6.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// bne cr6,0x82b813a4
	if (!ctx.cr6.eq) goto loc_82B813A4;
	// bl 0x82b7fd78
	ctx.lr = 0x82B8137C;
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
	ctx.lr = 0x82B813A0;
	sub_82B7FEC0(ctx, base);
	// b 0x82b81514
	goto loc_82B81514;
loc_82B813A4:
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// andi. r10,r11,131
	ctx.r[10].u64 = ctx.r[11].u64 & 131;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b81514
	if (ctx.cr0.eq) goto loc_82B81514;
	// rlwinm. r10,r11,0,25,25
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82b81514
	if (!ctx.cr0.eq) goto loc_82B81514;
	// rlwinm. r10,r11,0,30,30
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b813cc
	if (ctx.cr0.eq) goto loc_82B813CC;
	// ori r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 | 32;
	// b 0x82b81510
	goto loc_82B81510;
loc_82B813CC:
	// ori r11,r11,1
	ctx.r[11].u64 = ctx.r[11].u64 | 1;
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
	// andi. r10,r11,268
	ctx.r[10].u64 = ctx.r[11].u64 & 268;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// cmpwi r10,0
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x82b813ec
	if (!ctx.cr0.eq) goto loc_82B813EC;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b85c40
	ctx.lr = 0x82B813E8;
	sub_82B85C40(ctx, base);
	// b 0x82b813f4
	goto loc_82B813F4;
loc_82B813EC:
	// lwz r11,8(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_82B813F4:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r30,24(r31)
	ctx.r[30].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 24);
	// lwz r29,8(r31)
	ctx.r[29].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 8);
	// bl 0x82b81648
	ctx.lr = 0x82B81404;
	sub_82B81648(ctx, base);
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x82b85a80
	ctx.lr = 0x82B81410;
	sub_82B85A80(ctx, base);
	// cmpwi r3,0
	ctx.cr0.compare<int32_t>(ctx.r[3].s32, 0, ctx.xer);
	// stw r3,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[3].u32);
	// beq 0x82b814f0
	if (ctx.cr0.eq) goto loc_82B814F0;
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82b814f0
	if (ctx.cr6.eq) goto loc_82B814F0;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// andi. r11,r11,130
	ctx.r[11].u64 = ctx.r[11].u64 & 130;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmpwi r11,0
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b814a8
	if (!ctx.cr0.eq) goto loc_82B814A8;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B8143C;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-1
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -1, ctx.xer);
	// beq cr6,0x82b81484
	if (ctx.cr6.eq) goto loc_82B81484;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B8144C;
	sub_82B81648(ctx, base);
	// cmpwi cr6,r3,-2
	ctx.cr6.compare<int32_t>(ctx.r[3].s32, -2, ctx.xer);
	// beq cr6,0x82b81484
	if (ctx.cr6.eq) goto loc_82B81484;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82b81648
	ctx.lr = 0x82B8145C;
	sub_82B81648(ctx, base);
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r30,r11,-29312
	ctx.r[30].s64 = ctx.r[11].s64 + -29312;
	// srawi r11,r3,5
	ctx.xer.ca = std::uint8_t(ctx.r[3].s32 < 0 && (ctx.r[3].u32 & 0x1fu) != 0u);
	ctx.r[11].s64 = ArithmeticShiftRight32(ctx.r[3].s32, 5u);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// rlwinm r29,r11,2,0,29
	ctx.r[29].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// bl 0x82b81648
	ctx.lr = 0x82B81474;
	sub_82B81648(ctx, base);
	// lwzx r10,r29,r30
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[29].u32 + ctx.r[30].u32);
	// rlwinm r11,r3,6,21,25
	ctx.r[11].u64 = std::rotl(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 6) & 0x7C0;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// b 0x82b8148c
	goto loc_82B8148C;
loc_82B81484:
	// lis r11,-31967
	ctx.r[11].s64 = -2094989312;
	// addi r11,r11,21272
	ctx.r[11].s64 = ctx.r[11].s64 + 21272;
loc_82B8148C:
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// andi. r11,r11,130
	ctx.r[11].u64 = ctx.r[11].u64 & 130;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// cmplwi cr6,r11,130
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 130, ctx.xer);
	// bne cr6,0x82b814a8
	if (!ctx.cr6.eq) goto loc_82B814A8;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// ori r11,r11,8192
	ctx.r[11].u64 = ctx.r[11].u64 | 8192;
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
loc_82B814A8:
	// lwz r11,24(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 24);
	// cmpwi cr6,r11,512
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 512, ctx.xer);
	// bne cr6,0x82b814d0
	if (!ctx.cr6.eq) goto loc_82B814D0;
	// lwz r11,12(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// rlwinm. r10,r11,0,28,28
	ctx.r[10].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x8;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x82b814d0
	if (ctx.cr0.eq) goto loc_82B814D0;
	// rlwinm. r11,r11,0,21,21
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x400;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b814d0
	if (!ctx.cr0.eq) goto loc_82B814D0;
	// li r11,4096
	ctx.r[11].s64 = 4096;
	// stw r11,24(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 24, ctx.r[11].u32);
loc_82B814D0:
	// lwz r10,4(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 4);
	// lwz r11,0(r31)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 0);
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// stw r10,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[10].u32);
	// lbz r3,0(r11)
	ctx.r[3].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 0);
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// stw r11,0(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 0, ctx.r[11].u32);
	// b 0x82b81518
	goto loc_82B81518;
loc_82B814F0:
	// subfic r11,r3,0
	ctx.xer.ca = ctx.r[3].u32 <= 0;
	ctx.r[11].u64 = 0u - ctx.r[3].u64;
	// lwz r10,12(r31)
	ctx.r[10].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 12);
	// li r9,0
	ctx.r[9].s64 = 0;
	// subfe r11,r11,r11
	temp.u8 = ctx.xer.ca;
	ctx.r[11].u64 = temp.u8 ? 0u : UINT64_MAX;
	ctx.xer.ca = temp.u8;
	// rlwinm r11,r11,0,27,27
	ctx.r[11].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x10;
	// addi r11,r11,16
	ctx.r[11].s64 = ctx.r[11].s64 + 16;
	// stw r9,4(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 4, ctx.r[9].u32);
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
loc_82B81510:
	// stw r11,12(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 12, ctx.r[11].u32);
loc_82B81514:
	// li r3,-1
	ctx.r[3].s64 = -1;
loc_82B81518:
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	__restgprlr_29(ctx, base);
	return;
}
#undef PPC_STORE_U32
#undef PPC_LOAD_U32
#undef PPC_LOAD_U8
#undef __restgprlr_29
#undef __savegprlr_29
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (entry != 0x82b81360u) return false;
    Context ctx{};
    FromFull(ctx, state);
    Base base{memory, dependencies, state};
    Body(ctx, base);
    ToFull(state, ctx);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_byte_read_context
