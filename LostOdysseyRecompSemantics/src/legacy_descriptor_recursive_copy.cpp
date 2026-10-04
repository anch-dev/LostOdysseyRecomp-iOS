#include "lo_semantics/legacy_descriptor_recursive_copy.h"

#include "lo_semantics/legacy_descriptor_clone_chain.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_recursive_copy
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
std::uint64_t Load64(GuestMemory& m, std::uint64_t a)
{ return ReadU64(m, Address(a)); }
void Store64(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ WriteU64(m, Address(a), v); }
void CallLower(GuestAddress entry, Context& c, GuestMemory& m,
    Dependencies dependencies)
{
    Registers s{}; FromContext(c, s);
    if (entry == 0x82fb6b28u)
        (void)legacy_descriptor_clone_chain::Apply(entry, m,
            dependencies.selected, s);
    else
        (void)legacy_descriptor_value_intern::Apply(entry, m,
            dependencies.selected, s);
    c = ToContext(s);
}
void CallGuest(GuestAddress entry, Context& c, GuestMemory& m,
    Dependencies dependencies)
{
    Registers s{}; FromContext(c, s);
    dependencies.guest.Call(entry, m, s);
    c = ToContext(s);
}
void Body83026C80(Context& ctx, GuestMemory& memory,
    Dependencies dependencies)
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
	// stwu r1,-128(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-128);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r30,r4
	ctx.r[30].u64 = ctx.r[4].u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// lwz r10,4(r30)
	ctx.r[10].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// clrlwi r11,r10,30
	ctx.r[11].u64 = ctx.r[10].u32 & 0x3;
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 1, ctx.xer);
	// blt cr6,0x83026d7c
	if (ctx.cr6.lt) goto loc_83026D7C;
	// beq cr6,0x83026d44
	if (ctx.cr6.eq) goto loc_83026D44;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 3, ctx.xer);
	// blt cr6,0x83026cf8
	if (ctx.cr6.lt) goto loc_83026CF8;
	// beq cr6,0x83026cc4
	if (ctx.cr6.eq) goto loc_83026CC4;
	// li r4,4800
	ctx.r[4].s64 = 4800;
	// bl 0x82f99f48
	ctx.lr = 0x83026CC4;
	CallGuest(0x82f99f48u, ctx, memory, dependencies);
loc_83026CC4:
	// lwz r4,0(r30)
	ctx.r[4].u64 = Load32(memory, ctx.r[30].u32 + 0);
	// rlwinm. r11,r10,16,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 16) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x83026df8
	if (ctx.cr0.eq) goto loc_83026DF8;
	// li r6,0
	ctx.r[6].s64 = 0;
	// ld r5,0(r4)
	ctx.r[5].u64 = Load64(memory, ctx.r[4].u32 + 0);
	// bl 0x83025e68
	ctx.lr = 0x83026CDC;
	CallGuest(0x83025e68u, ctx, memory, dependencies);
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82fbd850
	ctx.lr = 0x83026CE8;
	CallLower(0x82fbd850u, ctx, memory, dependencies);
	// lwz r10,0(r3)
	ctx.r[10].u64 = Load32(memory, ctx.r[3].u32 + 0);
	// li r11,1
	ctx.r[11].s64 = 1;
	// rlwimi r10,r11,25,4,6
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 25) & 0xE000000) | (ctx.r[10].u64 & 0xFFFFFFFFF1FFFFFF);
	// b 0x83026d74
	goto loc_83026D74;
loc_83026CF8:
	// lwz r4,0(r30)
	ctx.r[4].u64 = Load32(memory, ctx.r[30].u32 + 0);
	// lwz r5,12(r4)
	ctx.r[5].u64 = Load32(memory, ctx.r[4].u32 + 12);
	// bl 0x82fb6b28
	ctx.lr = 0x83026D04;
	CallLower(0x82fb6b28u, ctx, memory, dependencies);
	// lwz r11,4(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// rlwinm. r11,r11,0,0,14
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFE0000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// lwz r11,0(r3)
	ctx.r[11].u64 = Load32(memory, ctx.r[3].u32 + 0);
	// rlwinm r10,r11,27,24,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0xFF;
	// bne 0x83026dfc
	if (!ctx.cr0.eq) goto loc_83026DFC;
	// li r9,1
	ctx.r[9].s64 = 1;
	// rlwimi r11,r9,25,4,6
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 25) & 0xE000000) | (ctx.r[11].u64 & 0xFFFFFFFFF1FFFFFF);
	// mr r9,r11
	ctx.r[9].u64 = ctx.r[11].u64;
	// stw r11,0(r3)
	Store32(memory, ctx.r[3].u32 + 0, ctx.r[11].u32);
	// lwz r11,4(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// rlwinm r11,r11,31,17,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 31) & 0x7FFE;
	// srw r11,r10,r11
	ctx.r[11].u64 = ctx.r[11].u8 & 0x20 ? 0 : (ctx.r[10].u32 >> (ctx.r[11].u8 & 0x3F));
	// rlwimi r9,r11,5,25,26
	ctx.r[9].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 5) & 0x60) | (ctx.r[9].u64 & 0xFFFFFFFFFFFFFF9F);
	// rlwinm r11,r9,0,25,18
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0xFFFFFFFFFFFFE07F;
	// stw r11,0(r3)
	Store32(memory, ctx.r[3].u32 + 0, ctx.r[11].u32);
	// b 0x83026dfc
	goto loc_83026DFC;
loc_83026D44:
	// lwz r4,0(r30)
	ctx.r[4].u64 = Load32(memory, ctx.r[30].u32 + 0);
	// bl 0x82fbd850
	ctx.lr = 0x83026D4C;
	CallLower(0x82fbd850u, ctx, memory, dependencies);
	// lwz r11,4(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// rlwinm. r11,r11,0,0,14
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFE0000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x83026dfc
	if (!ctx.cr0.eq) goto loc_83026DFC;
	// lwz r11,0(r3)
	ctx.r[11].u64 = Load32(memory, ctx.r[3].u32 + 0);
	// li r10,1
	ctx.r[10].s64 = 1;
	// rlwimi r11,r10,25,4,6
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 25) & 0xE000000) | (ctx.r[11].u64 & 0xFFFFFFFFF1FFFFFF);
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
	// stw r11,0(r3)
	Store32(memory, ctx.r[3].u32 + 0, ctx.r[11].u32);
	// lwz r11,4(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// rlwimi r10,r11,3,19,26
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 3) & 0x1FE0) | (ctx.r[10].u64 & 0xFFFFFFFFFFFFE01F);
loc_83026D74:
	// stw r10,0(r3)
	Store32(memory, ctx.r[3].u32 + 0, ctx.r[10].u32);
	// b 0x83026dfc
	goto loc_83026DFC;
loc_83026D7C:
	// lwz r11,4(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// clrlwi. r9,r11,31
	ctx.r[9].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x83026d8c
	if (ctx.cr0.eq) goto loc_83026D8C;
	// li r11,0
	ctx.r[11].s64 = 0;
loc_83026D8C:
	// lwz r5,548(r31)
	ctx.r[5].u64 = Load32(memory, ctx.r[31].u32 + 548);
	// cmplw cr6,r5,r11
	ctx.cr6.compare<uint32_t>(ctx.r[5].u32, ctx.r[11].u32, ctx.xer);
	// beq cr6,0x83026dd0
	if (ctx.cr6.eq) goto loc_83026DD0;
	// rlwinm r11,r10,15,17,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 15) & 0x7FFF;
	// lwz r10,12(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 12);
	// mulli r11,r11,40
	ctx.r[11].s64 = ctx.r[11].s64 * 40;
	// add r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 + ctx.r[10].u64;
	// lwz r11,4(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 4);
	// rlwinm. r11,r11,0,29,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x83026dd0
	if (ctx.cr0.eq) goto loc_83026DD0;
	// lwz r11,4(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// addi r3,r1,88
	ctx.r[3].s64 = ctx.r[1].s64 + 88;
	// rlwinm r7,r11,30,18,31
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 30) & 0x3FFF;
	// rlwinm r6,r11,15,17,31
	ctx.r[6].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 15) & 0x7FFF;
	// bl 0x830574d0
	ctx.lr = 0x83026DCC;
	CallGuest(0x830574d0u, ctx, memory, dependencies);
	// b 0x83026de8
	goto loc_83026DE8;
loc_83026DD0:
	// lwz r11,4(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// mr r4,r31
	ctx.r[4].u64 = ctx.r[31].u64;
	// addi r3,r1,96
	ctx.r[3].s64 = ctx.r[1].s64 + 96;
	// rlwinm r6,r11,30,18,31
	ctx.r[6].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 30) & 0x3FFF;
	// rlwinm r5,r11,15,17,31
	ctx.r[5].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 15) & 0x7FFF;
	// bl 0x8305b568
	ctx.lr = 0x83026DE8;
	CallGuest(0x8305b568u, ctx, memory, dependencies);
loc_83026DE8:
	// ld r11,0(r3)
	ctx.r[11].u64 = Load64(memory, ctx.r[3].u32 + 0);
	// addi r4,r1,80
	ctx.r[4].s64 = ctx.r[1].s64 + 80;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// std r11,80(r1)
	Store64(memory, ctx.r[1].u32 + 80, ctx.r[11].u64);
loc_83026DF8:
	// bl 0x83026c80
	ctx.lr = 0x83026DFC;
	Body83026C80(ctx, memory, dependencies);
loc_83026DFC:
	// addi r1,r1,128
	ctx.r[1].s64 = ctx.r[1].s64 + 128;
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
    if (entry != 0x83026c80u) return false;
    auto context = ToContext(registers);
    Body83026C80(context, memory, dependencies);
    FromContext(context, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_descriptor_recursive_copy
