#include "lo_semantics/legacy_descriptor_recursive_copy_caller.h"

#include "lo_semantics/legacy_descriptor_clone_chain.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_recursive_copy_caller
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
void Store64(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ WriteU64(m, Address(a), v); }
void Save19(Context& c, GuestMemory& m)
{
    for (unsigned i = 19u; i <= 31u; ++i)
        WriteU64(m, Address(c.r[1].u64 - 8u * (33u - i)), c.r[i].u64);
    Store32(m, c.r[1].u64 - 8u, c.r[12].u64);
}
void Restore19(Context& c, GuestMemory& m)
{
    for (unsigned i = 19u; i <= 31u; ++i)
        c.r[i].u64 = ReadU64(m, Address(c.r[1].u64 - 8u * (33u - i)));
    c.r[12].u64 = Load32(m, c.r[1].u64 - 8u);
    c.lr = c.r[12].u64;
}
void CallGuest(GuestAddress entry, Context& c, GuestMemory& m,
    Dependencies dependencies)
{
    Registers s{}; FromContext(c, s);
    dependencies.guest.Call(entry, m, s);
    c = ToContext(s);
}
void CallAccepted(GuestAddress entry, Context& c, GuestMemory& m,
    Dependencies dependencies)
{
    Registers s{}; FromContext(c, s);
    if (entry == 0x83026c80u)
        (void)legacy_descriptor_recursive_copy::Apply(entry, m,
            dependencies, s);
    else if (entry == 0x82fbda20u)
        (void)legacy_descriptor_clone_chain::Apply(entry, m,
            dependencies.selected, s);
    else
        (void)legacy_descriptor_value_intern::Apply(entry, m,
            dependencies.selected, s);
    c = ToContext(s);
}
void Body82FFD208(Context& ctx, GuestMemory& memory,
    Dependencies dependencies);
void Body82F99F48(Context& ctx, GuestMemory& memory,
    Dependencies dependencies);
void Body83026E18(Context& ctx, GuestMemory& memory,
    Dependencies dependencies);
void Body82FFD208(Context& ctx, GuestMemory& memory, Dependencies)
{
	// mr r11,r3
	ctx.r[11].u64 = ctx.r[3].u64;
	// lwz r9,8(r11)
	ctx.r[9].u64 = Load32(memory, ctx.r[11].u32 + 8);
	// lwz r10,4(r11)
	ctx.r[10].u64 = Load32(memory, ctx.r[11].u32 + 4);
	// addi r8,r9,1
	ctx.r[8].s64 = ctx.r[9].s64 + 1;
	// addi r9,r9,2
	ctx.r[9].s64 = ctx.r[9].s64 + 2;
	// rlwinm r9,r9,3,0,28
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 3) & 0xFFFFFFF8;
	// add r3,r9,r10
	ctx.r[3].u64 = ctx.r[9].u64 + ctx.r[10].u64;
	// stw r8,8(r11)
	Store32(memory, ctx.r[11].u32 + 8, ctx.r[8].u32);
	// lwz r9,8(r10)
	ctx.r[9].u64 = Load32(memory, ctx.r[10].u32 + 8);
	// cmplw cr6,r8,r9
	ctx.cr6.compare<uint32_t>(ctx.r[8].u32, ctx.r[9].u32, ctx.xer);
	// bnelr cr6
	if (!ctx.cr6.eq) return;
	// rlwinm r10,r10,0,0,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFE;
	// lwz r10,4(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 4);
	// clrlwi. r9,r10,31
	ctx.r[9].u64 = ctx.r[10].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x82ffd248
	if (ctx.cr0.eq) goto loc_82FFD248;
	// li r10,0
	ctx.r[10].s64 = 0;
loc_82FFD248:
	// stw r10,4(r11)
	Store32(memory, ctx.r[11].u32 + 4, ctx.r[10].u32);
	// li r10,0
	ctx.r[10].s64 = 0;
	// stw r10,8(r11)
	Store32(memory, ctx.r[11].u32 + 8, ctx.r[10].u32);
	// blr
	return;
}

void Body82F99F48(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// stw r12,-8(r1)
	Store32(memory, ctx.r[1].u32 + -8, ctx.r[12].u32);
	// std r5,32(r1)
	Store64(memory, ctx.r[1].u32 + 32, ctx.r[5].u64);
	// std r6,40(r1)
	Store64(memory, ctx.r[1].u32 + 40, ctx.r[6].u64);
	// std r7,48(r1)
	Store64(memory, ctx.r[1].u32 + 48, ctx.r[7].u64);
	// std r8,56(r1)
	Store64(memory, ctx.r[1].u32 + 56, ctx.r[8].u64);
	// std r9,64(r1)
	Store64(memory, ctx.r[1].u32 + 64, ctx.r[9].u64);
	// std r10,72(r1)
	Store64(memory, ctx.r[1].u32 + 72, ctx.r[10].u64);
	// stwu r1,-96(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-96);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// addi r11,r1,80
	ctx.r[11].s64 = ctx.r[1].s64 + 80;
	// addi r10,r1,128
	ctx.r[10].s64 = ctx.r[1].s64 + 128;
	// stw r10,0(r11)
	Store32(memory, ctx.r[11].u32 + 0, ctx.r[10].u32);
	// lwz r5,80(r1)
	ctx.r[5].u64 = Load32(memory, ctx.r[1].u32 + 80);
	// bl 0x82f99d98
	ctx.lr = 0x82F99F80;
	CallGuest(0x82f99d98u, ctx, memory, dependencies);
}

void Body83026E18(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6c4
	ctx.lr = 0x83026E20;
	Save19(ctx, memory);
	// stwu r1,-192(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-192);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r26,r4
	ctx.r[26].u64 = ctx.r[4].u64;
	// mr r29,r5
	ctx.r[29].u64 = ctx.r[5].u64;
	// mr r31,r3
	ctx.r[31].u64 = ctx.r[3].u64;
	// mr r19,r6
	ctx.r[19].u64 = ctx.r[6].u64;
	// mr r22,r7
	ctx.r[22].u64 = ctx.r[7].u64;
	// lwz r21,4(r26)
	ctx.r[21].u64 = Load32(memory, ctx.r[26].u32 + 4);
	// li r30,0
	ctx.r[30].s64 = 0;
	// lwz r20,8(r26)
	ctx.r[20].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// li r23,0
	ctx.r[23].s64 = 0;
	// li r28,0
	ctx.r[28].s64 = 0;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x83026f0c
	if (ctx.cr6.eq) goto loc_83026F0C;
	// li r24,0
	ctx.r[24].s64 = 0;
loc_83026E58:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82ffd208
	ctx.lr = 0x83026E60;
	Body82FFD208(ctx, memory, dependencies);
	// mr r27,r3
	ctx.r[27].u64 = ctx.r[3].u64;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x83026ed8
	if (ctx.cr6.eq) goto loc_83026ED8;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x83026b90
	ctx.lr = 0x83026E78;
	CallGuest(0x83026b90u, ctx, memory, dependencies);
	// mr r25,r3
	ctx.r[25].u64 = ctx.r[3].u64;
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x83026b90
	ctx.lr = 0x83026E88;
	CallGuest(0x83026b90u, ctx, memory, dependencies);
	// cmplw cr6,r25,r3
	ctx.cr6.compare<uint32_t>(ctx.r[25].u32, ctx.r[3].u32, ctx.xer);
	// bne cr6,0x83026f0c
	if (!ctx.cr6.eq) goto loc_83026F0C;
	// lwz r11,4(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// clrlwi r11,r11,30
	ctx.r[11].u64 = ctx.r[11].u32 & 0x3;
	// cmplwi cr6,r11,2
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 2, ctx.xer);
	// blt cr6,0x83026eac
	if (ctx.cr6.lt) goto loc_83026EAC;
	// beq cr6,0x83026fc4
	if (ctx.cr6.eq) goto loc_83026FC4;
	// cmplwi cr6,r11,4
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 4, ctx.xer);
	// bge cr6,0x83026fe4
	if (!ctx.cr6.lt) goto loc_83026FE4;
loc_83026EAC:
	// li r10,0
	ctx.r[10].s64 = 0;
loc_83026EB0:
	// lwz r11,4(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 4);
	// clrlwi r11,r11,30
	ctx.r[11].u64 = ctx.r[11].u32 & 0x3;
	// cmplwi cr6,r11,2
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 2, ctx.xer);
	// blt cr6,0x83026ecc
	if (ctx.cr6.lt) goto loc_83026ECC;
	// beq cr6,0x83026fd4
	if (ctx.cr6.eq) goto loc_83026FD4;
	// cmplwi cr6,r11,4
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 4, ctx.xer);
	// bge cr6,0x83026ff0
	if (!ctx.cr6.lt) goto loc_83026FF0;
loc_83026ECC:
	// li r11,0
	ctx.r[11].s64 = 0;
loc_83026ED0:
	// cmplw cr6,r10,r11
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, ctx.r[11].u32, ctx.xer);
	// bne cr6,0x83026f0c
	if (!ctx.cr6.eq) goto loc_83026F0C;
loc_83026ED8:
	// mr r4,r27
	ctx.r[4].u64 = ctx.r[27].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x83022948
	ctx.lr = 0x83026EE4;
	CallGuest(0x83022948u, ctx, memory, dependencies);
	// li r11,3
	ctx.r[11].s64 = 3;
	// addi r28,r28,1
	ctx.r[28].s64 = ctx.r[28].s64 + 1;
	// mr r30,r27
	ctx.r[30].u64 = ctx.r[27].u64;
	// cmplw cr6,r28,r29
	ctx.cr6.compare<uint32_t>(ctx.r[28].u32, ctx.r[29].u32, ctx.xer);
	// slw r10,r3,r24
	ctx.r[10].u64 = ctx.r[24].u8 & 0x20 ? 0 : (ctx.r[3].u32 << (ctx.r[24].u8 & 0x3F));
	// slw r11,r11,r24
	ctx.r[11].u64 = ctx.r[24].u8 & 0x20 ? 0 : (ctx.r[11].u32 << (ctx.r[24].u8 & 0x3F));
	// addi r24,r24,2
	ctx.r[24].s64 = ctx.r[24].s64 + 2;
	// andc r11,r23,r11
	ctx.r[11].u64 = ctx.r[23].u64 & ~ctx.r[11].u64;
	// or r23,r10,r11
	ctx.r[23].u64 = ctx.r[10].u64 | ctx.r[11].u64;
	// blt cr6,0x83026e58
	if (ctx.cr6.lt) goto loc_83026E58;
loc_83026F0C:
	// clrlwi. r11,r22,24
	ctx.r[11].u64 = ctx.r[22].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x83026ffc
	if (ctx.cr0.eq) goto loc_83026FFC;
	// cmplw cr6,r28,r29
	ctx.cr6.compare<uint32_t>(ctx.r[28].u32, ctx.r[29].u32, ctx.xer);
	// bge cr6,0x83026ffc
	if (!ctx.cr6.lt) goto loc_83026FFC;
	// mr r6,r29
	ctx.r[6].u64 = ctx.r[29].u64;
	// stw r21,4(r26)
	Store32(memory, ctx.r[26].u32 + 4, ctx.r[21].u32);
	// li r5,0
	ctx.r[5].s64 = 0;
	// stw r20,8(r26)
	Store32(memory, ctx.r[26].u32 + 8, ctx.r[20].u32);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// lwz r4,548(r31)
	ctx.r[4].u64 = Load32(memory, ctx.r[31].u32 + 548);
	// bl 0x83056cf8
	ctx.lr = 0x83026F38;
	CallGuest(0x83056cf8u, ctx, memory, dependencies);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// cmplwi cr6,r29,0
	ctx.cr6.compare<uint32_t>(ctx.r[29].u32, 0, ctx.xer);
	// beq cr6,0x83026f78
	if (ctx.cr6.eq) goto loc_83026F78;
	// addi r28,r30,40
	ctx.r[28].s64 = ctx.r[30].s64 + 40;
loc_83026F48:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82ffd208
	ctx.lr = 0x83026F50;
	Body82FFD208(ctx, memory, dependencies);
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x83026c80
	ctx.lr = 0x83026F5C;
	CallAccepted(0x83026c80u, ctx, memory, dependencies);
	// mr r4,r3
	ctx.r[4].u64 = ctx.r[3].u64;
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82fbda20
	ctx.lr = 0x83026F68;
	CallAccepted(0x82fbda20u, ctx, memory, dependencies);
	// stw r3,0(r28)
	Store32(memory, ctx.r[28].u32 + 0, ctx.r[3].u32);
	// addic. r29,r29,-1
	ctx.xer.ca = ctx.r[29].u32 > 0;
	ctx.r[29].s64 = ctx.r[29].s64 + -1;
	ctx.cr0.compare<int32_t>(ctx.r[29].s32, 0, ctx.xer);
	// addi r28,r28,4
	ctx.r[28].s64 = ctx.r[28].s64 + 4;
	// bne 0x83026f48
	if (!ctx.cr0.eq) goto loc_83026F48;
loc_83026F78:
	// lwz r10,548(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 548);
	// rlwinm r11,r30,0,0,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 0) & 0xFFFFFFFE;
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// addi r10,r10,24
	ctx.r[10].s64 = ctx.r[10].s64 + 24;
	// addi r11,r11,32
	ctx.r[11].s64 = ctx.r[11].s64 + 32;
	// addi r7,r10,-32
	ctx.r[7].s64 = ctx.r[10].s64 + -32;
	// addi r6,r11,-32
	ctx.r[6].s64 = ctx.r[11].s64 + -32;
	// ori r7,r7,1
	ctx.r[7].u64 = ctx.r[7].u64 | 1;
	// lwz r8,0(r10)
	ctx.r[8].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// addi r9,r11,4
	ctx.r[9].s64 = ctx.r[11].s64 + 4;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// stw r8,0(r11)
	Store32(memory, ctx.r[11].u32 + 0, ctx.r[8].u32);
	// lwz r8,0(r10)
	ctx.r[8].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// rlwinm r8,r8,0,0,30
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 0) & 0xFFFFFFFE;
	// stw r6,0(r8)
	Store32(memory, ctx.r[8].u32 + 0, ctx.r[6].u32);
	// stw r7,4(r11)
	Store32(memory, ctx.r[11].u32 + 4, ctx.r[7].u32);
	// stw r9,0(r10)
	Store32(memory, ctx.r[10].u32 + 0, ctx.r[9].u32);
	// bl 0x82fbd850
	ctx.lr = 0x83026FC0;
	CallAccepted(0x82fbd850u, ctx, memory, dependencies);
	// b 0x8302702c
	goto loc_8302702C;
loc_83026FC4:
	// lwz r11,0(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 0);
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// clrlwi r10,r11,27
	ctx.r[10].u64 = ctx.r[11].u32 & 0x1F;
	// b 0x83026eb0
	goto loc_83026EB0;
loc_83026FD4:
	// lwz r11,0(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 0);
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// clrlwi r11,r11,27
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1F;
	// b 0x83026ed0
	goto loc_83026ED0;
loc_83026FE4:
	// li r4,4800
	ctx.r[4].s64 = 4800;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82f99f48
	ctx.lr = 0x83026FF0;
	Body82F99F48(ctx, memory, dependencies);
loc_83026FF0:
	// li r4,4800
	ctx.r[4].s64 = 4800;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82f99f48
	ctx.lr = 0x83026FFC;
	Body82F99F48(ctx, memory, dependencies);
loc_83026FFC:
	// mr r4,r30
	ctx.r[4].u64 = ctx.r[30].u64;
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x83026c80
	ctx.lr = 0x83027008;
	CallAccepted(0x83026c80u, ctx, memory, dependencies);
	// clrlwi r10,r23,24
	ctx.r[10].u64 = ctx.r[23].u32 & 0xFF;
	// rlwinm r11,r28,20,9,11
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 20) & 0x700000;
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
	// lwz r10,0(r3)
	ctx.r[10].u64 = Load32(memory, ctx.r[3].u32 + 0);
	// rlwinm r10,r10,0,27,18
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFFFFFFE01F;
	// rlwinm r11,r11,5,0,26
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 5) & 0xFFFFFFE0;
	// rlwinm r10,r10,0,7,3
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFFF1FFFFFF;
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
	// stw r11,0(r3)
	Store32(memory, ctx.r[3].u32 + 0, ctx.r[11].u32);
loc_8302702C:
	// clrlwi. r11,r19,24
	ctx.r[11].u64 = ctx.r[19].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8302703c
	if (ctx.cr0.eq) goto loc_8302703C;
	// stw r21,4(r26)
	Store32(memory, ctx.r[26].u32 + 4, ctx.r[21].u32);
	// stw r20,8(r26)
	Store32(memory, ctx.r[26].u32 + 8, ctx.r[20].u32);
loc_8302703C:
	// addi r1,r1,192
	ctx.r[1].s64 = ctx.r[1].s64 + 192;
	// b 0x82b7a714
	Restore19(ctx, memory);
	return;
}

} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x83026e18u && entry != 0x82ffd208u &&
        entry != 0x82f99f48u) return false;
    auto context = ToContext(registers);
    if (entry == 0x83026e18u) Body83026E18(context, memory, dependencies);
    else if (entry == 0x82ffd208u)
        Body82FFD208(context, memory, dependencies);
    else Body82F99F48(context, memory, dependencies);
    FromContext(context, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_descriptor_recursive_copy_caller
