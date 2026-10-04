#include "lo_semantics/crt_stream_refill_context.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_stream_read_routes.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_refill_context
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
void Save25(Context& ctx, Base& base)
{
    for (unsigned i = 25; i <= 31; ++i)
        WriteU64(base.memory, Address(ctx.r[1].u64 - 16u - 8u * (31u - i)),
            ctx.r[i].u64);
    base.memory.WriteU32(Address(ctx.r[1].u64 - 8u), ctx.r[12].u32);
}
void Restore25(Context& ctx, Base& base)
{
    for (unsigned i = 25; i <= 31; ++i)
        ctx.r[i].u64 = ReadU64(base.memory,
            Address(ctx.r[1].u64 - 16u - 8u * (31u - i)));
    ctx.r[12].u64 = base.memory.ReadU32(Address(ctx.r[1].u64 - 8u));
    ctx.lr = ctx.r[12].u64;
}
void __savegprlr_25(Context& ctx, Base& base) { Save25(ctx, base); }
void __restgprlr_25(Context& ctx, Base& base) { Restore25(ctx, base); }
void Body_82B85A80(Context&, Base&);
void Body_82B85C08(Context&, Base&);

void Direct(GuestAddress entry, Context& ctx, Base& base)
{
    ToFull(base.full, ctx);
    if (entry == 0x82b85c08u)
    {
        Body_82B85C08(ctx, base);
        ToFull(base.full, ctx);
    }
    else if (entry == 0x82b85448u)
    {
        if (!crt_stream_read_routes::Apply(entry, base.memory,
                base.dependencies.open.open.read, base.full))
            throw std::logic_error("missing accepted CRT stream read");
    }
    else if (entry == 0x82b863f0u)
    {
        auto& full = base.full;
        crt_stream_pointer_unlock::Registers lower{full.r[1], full.lr,
            full.r[3], full.r[9], full.r[10], full.r[11], full.r[12],
            full.r[30], full.r[31], full.xer_ca};
        if (!crt_stream_pointer_unlock::Apply(entry, base.memory,
                base.dependencies.open.unlock, lower))
            throw std::logic_error("missing accepted pointer unlock");
        full.r[1] = lower.sp; full.lr = lower.lr;
        full.r[3] = lower.r3; full.r[9] = lower.r9;
        full.r[10] = lower.r10; full.r[11] = lower.r11;
        full.r[12] = lower.r12; full.r[30] = lower.r30;
        full.r[31] = lower.r31; full.xer_ca = lower.xer_ca;
    }
    else
    {
        auto lower = crt_context_adapter::ToStream(base.full);
        bool found = false;
        if (entry == 0x82b862f8u)
            found = crt_stream_open_routes_context::ApplyAcceptedLower(entry,
                base.memory, base.dependencies.open.open.stream,
                base.dependencies.open.open.open, lower);
        else
            found = crt_stream_operations::ApplyAcceptedCallee(entry,
                base.memory, base.dependencies.open.open.stream, lower);
        if (!found) throw std::logic_error("missing accepted CRT refill callee");
        crt_context_adapter::FromStream(base.full, lower);
    }
    FromFull(ctx, base.full);
}
void sub_82B7FDB0(Context& ctx, Base& base) { Direct(0x82B7FDB0u, ctx, base); }
void sub_82B7FD78(Context& ctx, Base& base) { Direct(0x82B7FD78u, ctx, base); }
void sub_82B7FEC0(Context& ctx, Base& base) { Direct(0x82B7FEC0u, ctx, base); }
void sub_82B862F8(Context& ctx, Base& base) { Direct(0x82B862F8u, ctx, base); }
void sub_82B85448(Context& ctx, Base& base) { Direct(0x82B85448u, ctx, base); }
void sub_82B85C08(Context& ctx, Base& base) { Direct(0x82B85C08u, ctx, base); }
void sub_82B863F0(Context& ctx, Base& base) { Direct(0x82B863F0u, ctx, base); }

#define PPC_LOAD_U8(a) base.memory.ReadU8(Address(a))
#define PPC_LOAD_U32(a) base.memory.ReadU32(Address(a))
#define PPC_LOAD_U64(a) ReadU64(base.memory,Address(a))
#define PPC_STORE_U8(a,v) base.memory.WriteU8(Address(a),v)
#define PPC_STORE_U32(a,v) base.memory.WriteU32(Address(a),v)
#define PPC_STORE_U64(a,v) WriteU64(base.memory,Address(a),v)
void Body_82B85A80(Context& ctx, Base& base) {
	PpcRegister temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6dc
	ctx.lr = 0x82B85A88;
	__savegprlr_25(ctx, base);
	// addi r31,r1,-160
	ctx.r[31].s64 = ctx.r[1].s64 + -160;
	// stwu r1,-160(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-160);
	PPC_STORE_U32(temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// stw r30,180(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 180, ctx.r[30].u32);
	// mr r25,r4
	ctx.r[25].u64 = ctx.r[4].u64;
	// mr r26,r5
	ctx.r[26].u64 = ctx.r[5].u64;
	// cmpwi cr6,r30,-2
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, -2, ctx.xer);
	// bne cr6,0x82b85acc
	if (!ctx.cr6.eq) goto loc_82B85ACC;
	// bl 0x82b7fdb0
	ctx.lr = 0x82B85AAC;
	sub_82B7FDB0(ctx, base);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fd78
	ctx.lr = 0x82B85AB8;
	sub_82B7FD78(ctx, base);
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// li r10,9
	ctx.r[10].s64 = 9;
	// li r3,-1
	ctx.r[3].s64 = -1;
	// stw r10,0(r11)
	PPC_STORE_U32(ctx.r[11].u32 + 0, ctx.r[10].u32);
	// b 0x82b85be0
	goto loc_82B85BE0;
loc_82B85ACC:
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// blt cr6,0x82b85ae4
	if (ctx.cr6.lt) goto loc_82B85AE4;
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// lwz r11,-29336(r11)
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[11].u32 + -29336);
	// cmplw cr6,r30,r11
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, ctx.r[11].u32, ctx.xer);
	// blt cr6,0x82b85b20
	if (ctx.cr6.lt) goto loc_82B85B20;
loc_82B85AE4:
	// bl 0x82b7fdb0
	ctx.lr = 0x82B85AE8;
	sub_82B7FDB0(ctx, base);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fd78
	ctx.lr = 0x82B85AF4;
	sub_82B7FD78(ctx, base);
	// li r10,9
	ctx.r[10].s64 = 9;
loc_82B85AF8:
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
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
	ctx.lr = 0x82B85B18;
	sub_82B7FEC0(ctx, base);
	// li r3,-1
	ctx.r[3].s64 = -1;
	// b 0x82b85be0
	goto loc_82B85BE0;
loc_82B85B20:
	// lis r11,-31944
	ctx.r[11].s64 = -2093481984;
	// addi r29,r11,-29312
	ctx.r[29].s64 = ctx.r[11].s64 + -29312;
	// srawi r11,r30,5
	ctx.xer.ca = static_cast<uint8_t>(ctx.r[30].s32 < 0) & static_cast<uint8_t>((ctx.r[30].u32 & 0x1F) != 0);
	ctx.r[11].s64 = ctx.r[30].s32 >> 5;
	// rlwinm r27,r11,2,0,29
	ctx.r[27].u64 = std::rotl(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r28,r30,6,21,25
	ctx.r[28].u64 = std::rotl(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 6) & 0x7C0;
	// lwzx r11,r27,r29
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + ctx.r[29].u32);
	// add r11,r11,r28
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[28].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b85ae4
	if (ctx.cr0.eq) goto loc_82B85AE4;
	// lis r11,32767
	ctx.r[11].s64 = 2147418112;
	// ori r11,r11,65535
	ctx.r[11].u64 = ctx.r[11].u64 | 65535;
	// subfc r11,r26,r11
	ctx.xer.ca = ctx.r[11].u32 >= ctx.r[26].u32;
	ctx.r[11].s64 = ctx.r[11].s64 - ctx.r[26].s64;
	// subfe r11,r11,r11
	temp.u8 = static_cast<uint8_t>(~ctx.r[11].u32 + ctx.r[11].u32 < ~ctx.r[11].u32) | static_cast<uint8_t>(~ctx.r[11].u32 + ctx.r[11].u32 + ctx.xer.ca < ctx.xer.ca);
	ctx.r[11].u64 = ~ctx.r[11].u64 + ctx.r[11].u64 + ctx.xer.ca;
	ctx.xer.ca = temp.u8;
	// addic. r11,r11,1
	ctx.xer.ca = ctx.r[11].u32 > 4294967294;
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x82b85b78
	if (!ctx.cr0.eq) goto loc_82B85B78;
	// bl 0x82b7fdb0
	ctx.lr = 0x82B85B64;
	sub_82B7FDB0(ctx, base);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fd78
	ctx.lr = 0x82B85B70;
	sub_82B7FD78(ctx, base);
	// li r10,22
	ctx.r[10].s64 = 22;
	// b 0x82b85af8
	goto loc_82B85AF8;
loc_82B85B78:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b862f8
	ctx.lr = 0x82B85B80;
	sub_82B862F8(ctx, base);
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// lwzx r11,r27,r29
	ctx.r[11].u64 = PPC_LOAD_U32(ctx.r[27].u32 + ctx.r[29].u32);
	// add r11,r11,r28
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[28].u64;
	// lbz r11,4(r11)
	ctx.r[11].u64 = PPC_LOAD_U8(ctx.r[11].u32 + 4);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x82b85bb0
	if (ctx.cr0.eq) goto loc_82B85BB0;
	// mr r5,r26
	ctx.r[5].u64 = ctx.r[26].u64;
	// mr r4,r25
	ctx.r[4].u64 = ctx.r[25].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b85448
	ctx.lr = 0x82B85BA8;
	sub_82B85448(ctx, base);
	// stw r3,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[3].u32);
	// b 0x82b85bd0
	goto loc_82B85BD0;
loc_82B85BB0:
	// bl 0x82b7fd78
	ctx.lr = 0x82B85BB4;
	sub_82B7FD78(ctx, base);
	// li r11,9
	ctx.r[11].s64 = 9;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// bl 0x82b7fdb0
	ctx.lr = 0x82B85BC0;
	sub_82B7FDB0(ctx, base);
	// li r11,0
	ctx.r[11].s64 = 0;
	// stw r11,0(r3)
	PPC_STORE_U32(ctx.r[3].u32 + 0, ctx.r[11].u32);
	// li r11,-1
	ctx.r[11].s64 = -1;
	// stw r11,80(r31)
	PPC_STORE_U32(ctx.r[31].u32 + 80, ctx.r[11].u32);
loc_82B85BD0:
	// mr r8,r8
	ctx.r[8].u64 = ctx.r[8].u64;
	// addi r12,r31,160
	ctx.r[12].s64 = ctx.r[31].s64 + 160;
	// bl 0x82b85c08
	ctx.lr = 0x82B85BDC;
	sub_82B85C08(ctx, base);
	// lwz r3,80(r31)
	ctx.r[3].u64 = PPC_LOAD_U32(ctx.r[31].u32 + 80);
loc_82B85BE0:
	// addi r1,r31,160
	ctx.r[1].s64 = ctx.r[31].s64 + 160;
	// b 0x82b7a72c
	__restgprlr_25(ctx, base);
	return;
}

void Body_82B85C08(Context& ctx, Base& base) {
	PpcRegister temp{};
	// std r31,-8(r1)
	PPC_STORE_U64(ctx.r[1].u32 + -8, ctx.r[31].u64);
	// addi r31,r12,-160
	ctx.r[31].s64 = ctx.r[12].s64 + -160;
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
	// bl 0x82b863f0
	ctx.lr = 0x82B85C28;
	sub_82B863F0(ctx, base);
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
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    Context ctx{}; FromFull(ctx, state); Base base{memory, dependencies, state};
    switch (entry)
    {
    case 0x82b85a80u: Body_82B85A80(ctx, base); break;
    case 0x82b85c08u: Body_82B85C08(ctx, base); break;
    default: return false;
    }
    ToFull(state, ctx);
    return true;
}
bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82b7fdb0u: case 0x82b7fd78u: case 0x82b7fec0u:
    case 0x82b862f8u: case 0x82b85448u: case 0x82b863f0u: break;
    default: return false;
    }
    Context ctx{}; FromFull(ctx, state); Base base{memory, dependencies, state};
    Direct(entry, ctx, base);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_refill_context
