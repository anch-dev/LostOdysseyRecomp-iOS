#include "lo_semantics/legacy_descriptor_attachment_callers.h"

#include "lo_semantics/legacy_descriptor_clone_chain.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_attachment_callers
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
    std::uint64_t f12 = 0, f13 = 0;
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
    s.cached_fp_control = c.fp_control;
}
std::uint32_t Load32(GuestMemory& m, std::uint64_t a)
{ return m.ReadU32(Address(a)); }
void Store32(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ m.WriteU32(Address(a), Address(v)); }
void Save(Context& c, GuestMemory& m, unsigned first)
{
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(m, Address(c.r[1].u64 - 8u * (33u - i)), c.r[i].u64);
    Store32(m, c.r[1].u64 - 8u, c.r[12].u64);
}
void Restore(Context& c, GuestMemory& m, unsigned first)
{
    for (unsigned i = first; i <= 31u; ++i)
        c.r[i].u64 = ReadU64(m, Address(c.r[1].u64 - 8u * (33u - i)));
    c.r[12].u64 = Load32(m, c.r[1].u64 - 8u);
    c.lr = c.r[12].u64;
}
void CallLower(GuestAddress entry, Context& c, GuestMemory& m,
    Services& services)
{
    Registers s{}; FromContext(c, s);
    if (entry == 0x82fbd850u)
        (void)legacy_descriptor_value_intern::Apply(entry, m, services, s);
    else
        (void)legacy_descriptor_clone_chain::Apply(entry, m, services, s);
    c = ToContext(s);
}

void Body83057B90(Context& ctx, GuestMemory& memory, Services& services)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e8
	ctx.lr = 0x83057B98;
	Save(ctx, memory, 28u);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r5
	ctx.r[30].u64 = ctx.r[5].u64;
	// li r8,1
	ctx.r[8].s64 = 1;
	// li r7,1
	ctx.r[7].s64 = 1;
	// li r6,48
	ctx.r[6].s64 = 48;
	// li r5,0
	ctx.r[5].s64 = 0;
	// mr r29,r3
	ctx.r[29].u64 = ctx.r[3].u64;
	// mr r28,r4
	ctx.r[28].u64 = ctx.r[4].u64;
	// bl 0x83056b90
	ctx.lr = 0x83057BBC;
	CallLower(0x83056b90u, ctx, memory, services);
	// lwz r11,16(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 16);
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x83057be0
	if (ctx.cr6.eq) goto loc_83057BE0;
	// mr r3,r29
	ctx.r[3].u64 = ctx.r[29].u64;
	// lwz r5,12(r30)
	ctx.r[5].u64 = Load32(memory, ctx.r[30].u32 + 12);
	// bl 0x82fb6b28
	ctx.lr = 0x83057BDC;
	CallLower(0x82fb6b28u, ctx, memory, services);
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
loc_83057BE0:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82fbda20
	ctx.lr = 0x83057BE8;
	CallLower(0x82fbda20u, ctx, memory, services);
	// mr r9,r3
	ctx.r[9].u64 = ctx.r[3].u64;
	// addi r11,r28,24
	ctx.r[11].s64 = ctx.r[28].s64 + 24;
	// rlwinm r10,r31,0,0,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 0) & 0xFFFFFFFE;
	// addi r7,r11,-32
	ctx.r[7].s64 = ctx.r[11].s64 + -32;
	// addi r10,r10,32
	ctx.r[10].s64 = ctx.r[10].s64 + 32;
	// stw r9,40(r31)
	Store32(memory, ctx.r[31].u32 + 40, ctx.r[9].u32);
	// ori r7,r7,1
	ctx.r[7].u64 = ctx.r[7].u64 | 1;
	// lwz r8,0(r11)
	ctx.r[8].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// addi r6,r10,-32
	ctx.r[6].s64 = ctx.r[10].s64 + -32;
	// addi r9,r10,4
	ctx.r[9].s64 = ctx.r[10].s64 + 4;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// stw r8,0(r10)
	Store32(memory, ctx.r[10].u32 + 0, ctx.r[8].u32);
	// lwz r8,0(r11)
	ctx.r[8].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// rlwinm r8,r8,0,0,30
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 0) & 0xFFFFFFFE;
	// stw r6,0(r8)
	Store32(memory, ctx.r[8].u32 + 0, ctx.r[6].u32);
	// stw r7,4(r10)
	Store32(memory, ctx.r[10].u32 + 4, ctx.r[7].u32);
	// stw r9,0(r11)
	Store32(memory, ctx.r[11].u32 + 0, ctx.r[9].u32);
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a738
	Restore(ctx, memory, 28u);
	return;
}
void Body83057FB0(Context& ctx, GuestMemory& memory, Services& services)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e4
	ctx.lr = 0x83057FB8;
	Save(ctx, memory, 27u);
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r27,r5
	ctx.r[27].u64 = ctx.r[5].u64;
	// mr r29,r6
	ctx.r[29].u64 = ctx.r[6].u64;
	// li r8,4
	ctx.r[8].s64 = 4;
	// li r7,2
	ctx.r[7].s64 = 2;
	// li r6,1
	ctx.r[6].s64 = 1;
	// li r5,0
	ctx.r[5].s64 = 0;
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// mr r28,r4
	ctx.r[28].u64 = ctx.r[4].u64;
	// bl 0x83056b90
	ctx.lr = 0x83057FE0;
	CallLower(0x83056b90u, ctx, memory, services);
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82fbd850
	ctx.lr = 0x83057FF0;
	CallLower(0x82fbd850u, ctx, memory, services);
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82fbda20
	ctx.lr = 0x83057FFC;
	CallLower(0x82fbda20u, ctx, memory, services);
	// stw r3,40(r31)
	Store32(memory, ctx.r[31].u32 + 40, ctx.r[3].u32);
	// lwz r11,16(r29)
	ctx.r[11].u64 = Load32(memory, ctx.r[29].u32 + 16);
	// mr r4,r29
	ctx.r[4].u64 = ctx.r[29].u64;
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x83058020
	if (ctx.cr6.eq) goto loc_83058020;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// lwz r5,12(r29)
	ctx.r[5].u64 = Load32(memory, ctx.r[29].u32 + 12);
	// bl 0x82fb6b28
	ctx.lr = 0x8305801C;
	CallLower(0x82fb6b28u, ctx, memory, services);
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
loc_83058020:
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82fbda20
	ctx.lr = 0x83058028;
	CallLower(0x82fbda20u, ctx, memory, services);
	// mr r9,r3
	ctx.r[9].u64 = ctx.r[3].u64;
	// lwz r8,40(r31)
	ctx.r[8].u64 = Load32(memory, ctx.r[31].u32 + 40);
	// addi r10,r28,24
	ctx.r[10].s64 = ctx.r[28].s64 + 24;
	// lwz r7,8(r31)
	ctx.r[7].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// rlwinm r11,r31,0,0,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[31].u32 | (ctx.r[31].u64 << 32), 0) & 0xFFFFFFFE;
	// addi r6,r10,-32
	ctx.r[6].s64 = ctx.r[10].s64 + -32;
	// addi r11,r11,32
	ctx.r[11].s64 = ctx.r[11].s64 + 32;
	// stw r9,44(r31)
	Store32(memory, ctx.r[31].u32 + 44, ctx.r[9].u32);
	// ori r6,r6,1
	ctx.r[6].u64 = ctx.r[6].u64 | 1;
	// lwz r9,0(r8)
	ctx.r[9].u64 = Load32(memory, ctx.r[8].u32 + 0);
	// addi r5,r11,-32
	ctx.r[5].s64 = ctx.r[11].s64 + -32;
	// addi r8,r11,4
	ctx.r[8].s64 = ctx.r[11].s64 + 4;
	// rlwinm r9,r9,7,29,31
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 7) & 0x7;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// rlwimi r7,r9,14,15,17
	ctx.r[7].u64 = (__builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 14) & 0x1C000) | (ctx.r[7].u64 & 0xFFFFFFFFFFFE3FFF);
	// stw r7,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[7].u32);
	// lwz r9,0(r10)
	ctx.r[9].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// stw r9,0(r11)
	Store32(memory, ctx.r[11].u32 + 0, ctx.r[9].u32);
	// lwz r9,0(r10)
	ctx.r[9].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// rlwinm r9,r9,0,0,30
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0xFFFFFFFE;
	// stw r5,0(r9)
	Store32(memory, ctx.r[9].u32 + 0, ctx.r[5].u32);
	// stw r6,4(r11)
	Store32(memory, ctx.r[11].u32 + 4, ctx.r[6].u32);
	// stw r8,0(r10)
	Store32(memory, ctx.r[10].u32 + 0, ctx.r[8].u32);
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
	// b 0x82b7a734
	Restore(ctx, memory, 27u);
	return;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Services& services, Registers& registers)
{
    if (entry != 0x83057b90u && entry != 0x83057fb0u) return false;
    auto context = ToContext(registers);
    if (entry == 0x83057b90u) Body83057B90(context, memory, services);
    else Body83057FB0(context, memory, services);
    FromContext(context, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_descriptor_attachment_callers
