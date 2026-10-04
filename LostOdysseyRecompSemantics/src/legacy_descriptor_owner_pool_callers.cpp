#include "lo_semantics/legacy_descriptor_owner_pool_callers.h"

#include "lo_semantics/memory_fill.h"
#include "lo_semantics/raw_allocation_context.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <cstdlib>

namespace lo::semantic::gpu::legacy_descriptor_owner_pool_callers
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
union Reg
{
    std::uint64_t u64 = 0;
    std::int64_t s64;
    std::uint32_t u32;
    std::int32_t s32;
    std::uint8_t u8;
};
struct Xer { std::uint8_t ca = 0, so = 0; };
struct Condition
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
    Reg r[32]{};
    std::uint64_t lr = 0, ctr = 0;
    Xer xer{};
    Condition cr0{}, cr6{};
    std::uint64_t f0 = 0, f1 = 0, f2 = 0, f3 = 0, f4 = 0;
    std::uint64_t f12 = 0, f13 = 0, f30 = 0, f31 = 0;
    std::uint32_t fp_control = 0;
};
Context ToContext(const Registers& s)
{
    Context c{}; const auto& g = s.integer;
    for (unsigned i = 0; i < 32u; ++i) c.r[i].u64 = g.r[i];
    c.r[1].u64 = g.sp; c.lr = g.lr; c.ctr = g.ctr;
    c.xer = {g.xer_ca, g.xer_so};
    c.cr0 = {g.cr0.lt, g.cr0.gt, g.cr0.eq, g.cr0.so};
    c.cr6 = {g.cr6.lt, g.cr6.gt, g.cr6.eq, g.cr6.so};
    c.f0 = s.f0_bits; c.f1 = s.f1_bits; c.f2 = s.f2_bits;
    c.f3 = s.f3_bits; c.f4 = s.f4_bits;
    c.f12 = s.f12_bits; c.f13 = s.f13_bits;
    c.f30 = s.f30_bits; c.f31 = s.f31_bits;
    c.fp_control = s.cached_fp_control;
    return c;
}
void FromContext(const Context& c, Registers& s)
{
    auto& g = s.integer;
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) g.r[i] = c.r[i].u64;
    g.sp = c.r[1].u64; g.lr = c.lr; g.ctr = c.ctr;
    g.xer_ca = c.xer.ca; g.xer_so = c.xer.so;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    s.f0_bits = c.f0; s.f1_bits = c.f1; s.f2_bits = c.f2;
    s.f3_bits = c.f3; s.f4_bits = c.f4;
    s.f12_bits = c.f12; s.f13_bits = c.f13;
    s.f30_bits = c.f30; s.f31_bits = c.f31;
    s.cached_fp_control = c.fp_control;
}
std::uint32_t Load32(GuestMemory& m, std::uint64_t a)
{ return m.ReadU32(Address(a)); }
std::uint64_t Load64(GuestMemory& m, std::uint64_t a)
{ return ReadU64(m, Address(a)); }
void Store32(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ m.WriteU32(Address(a), Address(v)); }
void Store64(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ WriteU64(m, Address(a), v); }
void Save29(Context& c, GuestMemory& m)
{
    for (unsigned i = 29u; i <= 31u; ++i)
        Store64(m, c.r[1].u64 - 8u * (33u - i), c.r[i].u64);
    Store32(m, c.r[1].u64 - 8u, c.r[12].u64);
}
void Restore29(Context& c, GuestMemory& m)
{
    for (unsigned i = 29u; i <= 31u; ++i)
        c.r[i].u64 = Load64(m, c.r[1].u64 - 8u * (33u - i));
    c.r[12].u64 = Load32(m, c.r[1].u64 - 8u);
    c.lr = c.r[12].u64;
}
void CallGuest(GuestAddress entry, Context& c, GuestMemory& m,
    Dependencies d)
{
    Registers s{}; FromContext(c, s);
    d.guest.Call(entry, m, s);
    c = ToContext(s);
}
void CallFill(Context& c, GuestMemory& m)
{
    const auto destination = Address(c.r[3].u64);
    const auto bytes = c.r[5].u32;
    const auto padding = (0u - destination) & 3u;
    const auto prefix = bytes < padding ? bytes : padding;
    const auto remaining = bytes - prefix;
    (void)FillGuestMemory(m, destination, c.r[4].u32, bytes);
    c.r[6].u64 = c.r[3].u64 + prefix + (remaining & ~3u);
    c.r[5].u64 -= prefix;
    c.r[4].u64 = (c.r[4].u64 & 0xffffffff00000000ull) |
        (std::uint32_t{c.r[4].u8} * 0x01010101u);
    c.r[0].u64 = remaining & 3u;
    c.cr0.compare<std::int32_t>(c.r[0].s32, 0, c.xer);
    c.ctr = c.r[0].u32 == 3u ? 1u : 0u;
}
class UnusedRawDirect final : public raw_allocation_context::PpcBoundaryServices
{
    void CallDirect(GuestAddress, GuestMemory&,
        raw_allocation_context::Registers&) override { std::abort(); }
};
void CallHeap(GuestAddress entry, Context& c, GuestMemory& m,
    Dependencies d)
{
    heap_allocation_context::Registers state{};
    for (unsigned i = 0; i < 32u; ++i) state.r[i] = c.r[i].u64;
    state.lr = c.lr; state.ctr = c.ctr;
    state.xer_ca = c.xer.ca; state.xer_so = c.xer.so;
    state.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.f0_bits = c.f0; state.f1_bits = c.f1;
    state.f13_bits = c.f13; state.f30_bits = c.f30;
    state.f31_bits = c.f31; state.cached_fp_control = c.fp_control;
    if (entry == 0x823acc98u)
    {
        UnusedRawDirect no_direct;
        (void)raw_allocation_context::Apply(entry, m, no_direct, state);
    }
    else (void)heap_allocation_context::Apply(entry, m, d.heap, state);
    for (unsigned i = 0; i < 32u; ++i) c.r[i].u64 = state.r[i];
    c.lr = state.lr; c.ctr = state.ctr;
    c.xer = {state.xer_ca, state.xer_so};
    c.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.un};
    c.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.un};
    c.f0 = state.f0_bits; c.f1 = state.f1_bits;
    c.f13 = state.f13_bits; c.f30 = state.f30_bits;
    c.f31 = state.f31_bits; c.fp_control = state.cached_fp_control;
}

void Body827C9D88(Context&, GuestMemory&, Dependencies);
void Body827CA050(Context&, GuestMemory&, Dependencies);
void Body827CAD38(Context&, GuestMemory&, Dependencies);
void Body82FD1240(Context&, GuestMemory&, Dependencies);
void Body82FACBF0(Context&, GuestMemory&, Dependencies);
void Body827C9D88(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	// lis r11,-22654
	ctx.r[11].s64 = -1484652544;
	// ori r11,r11,7
	ctx.r[11].u64 = ctx.r[11].u64 | 7;
	// cmplw cr6,r4,r11
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, ctx.r[11].u32, ctx.xer);
	// bne cr6,0x827c9dac
	if (!ctx.cr6.eq) goto loc_827C9DAC;
	// lis r11,-31964
	ctx.r[11].s64 = -2094792704;
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// addi r11,r11,29096
	ctx.r[11].s64 = ctx.r[11].s64 + 29096;
	// mr r3,r11
	ctx.r[3].u64 = ctx.r[11].u64;
	// b 0x827c9a40
	CallGuest(0x827c9a40u, ctx, memory, dependencies);
	return;
loc_827C9DAC:
	// b 0x827ca050
	Body827CA050(ctx, memory, dependencies);
	return;
}
void Body827CA050(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6ec
	ctx.lr = 0x827CA058;
	Save29(ctx, memory);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r4
	ctx.r[31].u64 = ctx.r[4].u64;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// rlwinm. r11,r31,0,0,0
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 0) & 0x80000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x827ca0c8
	if (ctx.cr0.eq) goto loc_827CA0C8;
	// rlwinm. r11,r31,0,4,7
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 0) & 0xF000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x827ca07c
	if (!ctx.cr0.eq) goto loc_827CA07C;
	// li r11,3
	ctx.r[11].s64 = 3;
	// rlwimi r31,r11,26,4,7
	ctx.r[31].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 26) & 0xF000000) | (ctx.r[31].u64 & 0xFFFFFFFFF0FFFFFF);
loc_827CA07C:
	// rlwinm r10,r31,8,28,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 8) & 0xF;
	// li r9,1
	ctx.r[9].s64 = 1;
	// lis r11,-31970
	ctx.r[11].s64 = -2095185920;
	// rlwinm r8,r31,6,28,29
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 6) & 0xC;
	// addi r11,r11,30756
	ctx.r[11].s64 = ctx.r[11].s64 + 30756;
	// li r4,-1
	ctx.r[4].s64 = -1;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// lwzx r6,r8,r11
	ctx.r[6].u64 = Load32(memory, ctx.r[8].u32 + ctx.r[11].u32);
	// slw r5,r9,r10
	ctx.r[5].u64 = ctx.r[10].u8 & 0x20 ? 0 : (ctx.r[9].u32 << (ctx.r[10].u8 & 0x3F));
	// bl 0x827c9e20
	ctx.lr = 0x827CA0A4;
	CallGuest(0x827c9e20u, ctx, memory, dependencies);
	// mr. r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	ctx.cr0.compare<int32_t>(ctx.r[30].s32, 0, ctx.xer);
	// beq 0x827ca0d8
	if (ctx.cr0.eq) goto loc_827CA0D8;
	// rlwinm. r11,r31,0,1,1
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 0) & 0x40000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x827ca0d8
	if (ctx.cr0.eq) goto loc_827CA0D8;
	// mr r5,r29
	ctx.r[5].u64 = ctx.r[29].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82b7bc40
	ctx.lr = 0x827CA0C4;
	CallFill(ctx, memory);
	// b 0x827ca0d8
	goto loc_827CA0D8;
loc_827CA0C8:
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// rlwinm r3,r31,8,25,25
	ctx.r[3].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 8) & 0x40;
	// bl 0x827cad38
	ctx.lr = 0x827CA0D4;
	Body827CAD38(ctx, memory, dependencies);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
loc_827CA0D8:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// b 0x82b7a73c
	Restore29(ctx, memory);
	return;
}
void Body827CAD38(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	Store32(memory, ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r30,-24(r1)
	Store64(memory, ctx.r[1].u32 + -24, ctx.r[30].u64);
	// std r31,-16(r1)
	Store64(memory, ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// bl 0x823acc98
	ctx.lr = 0x827CAD58;
	CallHeap(0x823acc98u, ctx, memory, dependencies);
	// rlwinm r4,r31,29,28,28
	ctx.r[4].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 29) & 0x8;
	// mr r5,r30
	ctx.r[5].u64 = ctx.r[30].u64;
	// bl 0x823accb0
	ctx.lr = 0x827CAD64;
	CallHeap(0x823accb0u, ctx, memory, dependencies);
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = Load32(memory, ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r30,-24(r1)
	ctx.r[30].u64 = Load64(memory, ctx.r[1].u32 + -24);
	// ld r31,-16(r1)
	ctx.r[31].u64 = Load64(memory, ctx.r[1].u32 + -16);
	// blr
	return;
}
void Body82FD1240(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	Store32(memory, ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r31,-16(r1)
	Store64(memory, ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// addi r11,r3,772
	ctx.r[11].s64 = ctx.r[3].s64 + 772;
	// cmplwi cr6,r4,132
	ctx.cr6.compare<uint32_t>(ctx.r[4].u32, 132, ctx.xer);
	// ble cr6,0x82fd1268
	if (!ctx.cr6.gt) goto loc_82FD1268;
	// mr r3,r11
	ctx.r[3].u64 = ctx.r[11].u64;
	// bl 0x82facbf0
	ctx.lr = 0x82FD1264;
	Body82FACBF0(ctx, memory, dependencies);
	// b 0x82fd12d4
	goto loc_82FD12D4;
loc_82FD1268:
	// lwz r10,144(r11)
	ctx.r[10].u64 = Load32(memory, ctx.r[11].u32 + 144);
	// lwz r9,140(r11)
	ctx.r[9].u64 = Load32(memory, ctx.r[11].u32 + 140);
	// subf r9,r10,r9
	ctx.r[9].s64 = ctx.r[9].s64 - ctx.r[10].s64;
	// addi r9,r9,4096
	ctx.r[9].s64 = ctx.r[9].s64 + 4096;
	// cmplw cr6,r9,r4
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, ctx.r[4].u32, ctx.xer);
	// blt cr6,0x82fd128c
	if (ctx.cr6.lt) goto loc_82FD128C;
	// add r9,r10,r4
	ctx.r[9].u64 = ctx.r[10].u64 + ctx.r[4].u64;
	// stw r9,144(r11)
	Store32(memory, ctx.r[11].u32 + 144, ctx.r[9].u32);
	// b 0x82fd12d0
	goto loc_82FD12D0;
loc_82FD128C:
	// rlwinm r10,r4,30,2,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[4].u32 | (ctx.r[4].u64 << 32), 30) & 0x3FFFFFFF;
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// rlwinm r10,r10,2,0,29
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r31,r10,r11
	ctx.r[31].u64 = Load32(memory, ctx.r[10].u32 + ctx.r[11].u32);
	// cmplwi r31,0
	ctx.cr0.compare<uint32_t>(ctx.r[31].u32, 0, ctx.xer);
	// beq 0x82fd12c4
	if (ctx.cr0.eq) goto loc_82FD12C4;
	// lwz r9,0(r31)
	ctx.r[9].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// mr r5,r4
	ctx.r[5].u64 = ctx.r[4].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// stwx r9,r10,r11
	Store32(memory, ctx.r[10].u32 + ctx.r[11].u32, ctx.r[9].u32);
	// bl 0x82b7bc40
	ctx.lr = 0x82FD12BC;
	CallFill(ctx, memory);
	// mr r10,r31
	ctx.r[10].u64 = ctx.r[31].u64;
	// b 0x82fd12d0
	goto loc_82FD12D0;
loc_82FD12C4:
	// mr r3,r11
	ctx.r[3].u64 = ctx.r[11].u64;
	// bl 0x82fac428
	ctx.lr = 0x82FD12CC;
	CallGuest(0x82fac428u, ctx, memory, dependencies);
	// mr r10,r3
	ctx.r[10].u64 = ctx.r[3].u64;
loc_82FD12D0:
	// mr r3,r10
	ctx.r[3].u64 = ctx.r[10].u64;
loc_82FD12D4:
	// addi r1,r1,96
	ctx.r[1].s64 = ctx.r[1].s64 + 96;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = Load32(memory, ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r31,-16(r1)
	ctx.r[31].u64 = Load64(memory, ctx.r[1].u32 + -16);
	// blr
	return;
}
void Body82FACBF0(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	Store32(memory, ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r30,-24(r1)
	Store64(memory, ctx.r[1].u32 + -24, ctx.r[30].u64);
	// std r31,-16(r1)
	Store64(memory, ctx.r[1].u32 + -16, ctx.r[31].u64);
	// stwu r1,-112(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-112);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// addi r31,r4,12
	ctx.r[31].s64 = ctx.r[4].s64 + 12;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// lis r4,24973
	ctx.r[4].s64 = 1636630528;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x827c9d88
	ctx.lr = 0x82FACC18;
	Body827C9D88(ctx, memory, dependencies);
	// addi r11,r30,152
	ctx.r[11].s64 = ctx.r[30].s64 + 152;
	// stw r31,8(r3)
	Store32(memory, ctx.r[3].u32 + 8, ctx.r[31].u32);
	// rlwinm r10,r3,0,0,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 0) & 0xFFFFFFFE;
	// ori r7,r11,1
	ctx.r[7].u64 = ctx.r[11].u64 | 1;
	// addi r9,r10,4
	ctx.r[9].s64 = ctx.r[10].s64 + 4;
	// addi r3,r3,12
	ctx.r[3].s64 = ctx.r[3].s64 + 12;
	// lwz r8,0(r11)
	ctx.r[8].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// stw r8,0(r10)
	Store32(memory, ctx.r[10].u32 + 0, ctx.r[8].u32);
	// lwz r8,0(r11)
	ctx.r[8].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// rlwinm r8,r8,0,0,30
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 0) & 0xFFFFFFFE;
	// stw r10,0(r8)
	Store32(memory, ctx.r[8].u32 + 0, ctx.r[10].u32);
	// stw r7,4(r10)
	Store32(memory, ctx.r[10].u32 + 4, ctx.r[7].u32);
	// stw r9,0(r11)
	Store32(memory, ctx.r[11].u32 + 0, ctx.r[9].u32);
	// addi r1,r1,112
	ctx.r[1].s64 = ctx.r[1].s64 + 112;
	// lwz r12,-8(r1)
	ctx.r[12].u64 = Load32(memory, ctx.r[1].u32 + -8);
	// mtlr r12
	ctx.lr = ctx.r[12].u64;
	// ld r30,-24(r1)
	ctx.r[30].u64 = Load64(memory, ctx.r[1].u32 + -24);
	// ld r31,-16(r1)
	ctx.r[31].u64 = Load64(memory, ctx.r[1].u32 + -16);
	// blr
	return;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x82fd1240u && entry != 0x82facbf0u) return false;
    auto context = ToContext(registers);
    if (entry == 0x82fd1240u) Body82FD1240(context, memory, dependencies);
    else Body82FACBF0(context, memory, dependencies);
    FromContext(context, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_descriptor_owner_pool_callers
