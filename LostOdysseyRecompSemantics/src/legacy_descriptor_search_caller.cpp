#include "lo_semantics/legacy_descriptor_search_caller.h"

#include "lo_semantics/legacy_descriptor_layout_routes.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_search_caller
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

// The original body changes scratch GPRs and both CR fields between guest
// accesses. A local register view keeps those writes in instruction order.
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
};
Context ToContext(const Registers& s)
{
    Context c{};
    for (unsigned i = 0; i < 32u; ++i) c.r[i].u64 = s.r[i];
    c.r[1].u64 = s.sp;
    c.lr = s.lr; c.ctr = s.ctr;
    c.xer = {s.xer_ca, s.xer_so};
    c.cr0 = {s.cr0.lt, s.cr0.gt, s.cr0.eq, s.cr0.so};
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, s.cr6.so};
    return c;
}
void FromContext(const Context& c, Registers& s)
{
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) s.r[i] = c.r[i].u64;
    s.sp = c.r[1].u64;
    s.lr = c.lr; s.ctr = c.ctr;
    s.xer_ca = c.xer.ca; s.xer_so = c.xer.so;
    s.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
}
std::uint32_t Load32(GuestMemory& m, std::uint64_t a)
{ return m.ReadU32(Address(a)); }
std::uint8_t Load8(GuestMemory& m, std::uint64_t a)
{ return m.ReadU8(Address(a)); }
void Store32(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ m.WriteU32(Address(a), Address(v)); }
void Store8(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ m.WriteU8(Address(a), std::uint8_t(v)); }
void Save26(Context& c, GuestMemory& m)
{
    for (unsigned i = 26u; i <= 31u; ++i)
        WriteU64(m, Address(c.r[1].u64 - (33u - i) * 8u), c.r[i].u64);
    Store32(m, c.r[1].u64 - 8u, c.r[12].u64);
}
void Restore26(Context& c, GuestMemory& m)
{
    for (unsigned i = 26u; i <= 31u; ++i)
        c.r[i].u64 = ReadU64(m, Address(c.r[1].u64 - (33u - i) * 8u));
    c.r[12].u64 = Load32(m, c.r[1].u64 - 8u);
    c.lr = c.r[12].u64;
}
void CallLower(GuestAddress entry, Context& c, GuestMemory& m)
{
    auto s = Registers{};
    FromContext(c, s);
    const bool found = entry == 0x83054648u
        ? legacy_descriptor_layout_routes::Apply(entry, m, s)
        : legacy_descriptor_search_helpers::Apply(entry, m, s);
    if (found) c = ToContext(s);
}
void CallDiagnostic(Context& c, GuestMemory& m,
    DiagnosticServices& diagnostic)
{
    c.r[12].u64 = c.lr;
    Store32(m, c.r[1].u64 - 8u, c.r[12].u64);
    for (unsigned i = 5u; i <= 10u; ++i)
        WriteU64(m, Address(c.r[1].u64 + 32u + (i - 5u) * 8u),
            c.r[i].u64);
    Store32(m, c.r[1].u64 - 96u, c.r[1].u64);
    c.r[1].u64 -= 96u;
    c.r[11].u64 = c.r[1].u64 + 80u;
    c.r[10].u64 = c.r[1].u64 + 128u;
    Store32(m, c.r[11].u64, c.r[10].u64);
    c.r[5].u64 = Load32(m, c.r[1].u64 + 80u);
    c.lr = 0x82f99f80u;
    auto s = Registers{};
    FromContext(c, s);
    diagnostic.Call(0x82f99d98u, m, s);
    c = ToContext(s);
}

void Body(Context& ctx, GuestMemory& memory,
    DiagnosticServices& diagnostic)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6e0
	ctx.lr = 0x83058FD0;
	Save26(ctx, memory);
	// stwu r1,-144(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-144);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r26,r3
	ctx.r[26].u64 = ctx.r[3].u64;
	// mr r31,r4
	ctx.r[31].u64 = ctx.r[4].u64;
	// mr r28,r5
	ctx.r[28].u64 = ctx.r[5].u64;
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm. r11,r11,15,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 15) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x83059008
	if (ctx.cr0.eq) goto loc_83059008;
	// lwz r11,4(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// oris r11,r11,4096
	ctx.r[11].u64 = ctx.r[11].u64 | 268435456;
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
	// stw r11,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[11].u32);
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwimi r10,r11,9,4,4
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 9) & 0x8000000) | (ctx.r[10].u64 & 0xFFFFFFFFF7FFFFFF);
	// b 0x83059048
	goto loc_83059048;
loc_83059008:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82fac070
	ctx.lr = 0x83059010;
	CallLower(0x82fac070u, ctx, memory);
	// clrlwi. r11,r3,24
	ctx.r[11].u64 = ctx.r[3].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x8305904c
	if (!ctx.cr0.eq) goto loc_8305904C;
	// lwz r11,24(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 24);
	// lwz r11,76(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 76);
	// rlwinm. r11,r11,10,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 10) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8305904c
	if (ctx.cr0.eq) goto loc_8305904C;
	// lwz r11,4(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// oris r11,r11,4096
	ctx.r[11].u64 = ctx.r[11].u64 | 268435456;
	// mr r10,r11
	ctx.r[10].u64 = ctx.r[11].u64;
	// stw r11,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[11].u32);
	// lwz r11,24(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 24);
	// lwz r11,76(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 76);
	// rlwinm r11,r11,9,24,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 9) & 0xFF;
	// rlwimi r10,r11,27,4,4
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x8000000) | (ctx.r[10].u64 & 0xFFFFFFFFF7FFFFFF);
loc_83059048:
	// stw r10,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[10].u32);
loc_8305904C:
	// lwz r10,4(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// rlwinm. r11,r10,0,3,3
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x10000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x830590a0
	if (ctx.cr0.eq) goto loc_830590A0;
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm. r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x830590a0
	if (ctx.cr0.eq) goto loc_830590A0;
	// lwz r11,4(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 4);
loc_83059068:
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x83059084
	if (ctx.cr6.eq) goto loc_83059084;
	// lwz r9,0(r11)
	ctx.r[9].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// rlwinm. r9,r9,0,4,6
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0xE000000;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// bne 0x83059084
	if (!ctx.cr0.eq) goto loc_83059084;
	// lwz r11,8(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 8);
	// b 0x83059068
	goto loc_83059068;
loc_83059084:
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lis r9,64
	ctx.r[9].s64 = 4194304;
	// rlwinm r11,r11,0,7,14
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x1FE0000;
	// cmplw cr6,r11,r9
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[9].u32, ctx.xer);
	// bne cr6,0x830590a0
	if (!ctx.cr6.eq) goto loc_830590A0;
	// rlwinm r11,r10,0,5,2
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFFE7FFFFFF;
	// stw r11,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[11].u32);
loc_830590A0:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// li r29,0
	ctx.r[29].s64 = 0;
	// rlwinm r11,r11,25,25,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 25) & 0x7F;
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 1, ctx.xer);
	// blt cr6,0x830590c0
	if (ctx.cr6.lt) goto loc_830590C0;
	// cmplwi cr6,r11,30
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 30, ctx.xer);
	// li r11,1
	ctx.r[11].s64 = 1;
	// ble cr6,0x830590c4
	if (!ctx.cr6.gt) goto loc_830590C4;
loc_830590C0:
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
loc_830590C4:
	// clrlwi. r11,r11,24
	ctx.r[11].u64 = ctx.r[11].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x830593a4
	if (ctx.cr0.eq) goto loc_830593A4;
	// lwz r10,8(r26)
	ctx.r[10].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// lwz r11,0(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwinm r10,r10,25,7,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 25) & 0x1FFFFFF;
	// lwz r9,8(r31)
	ctx.r[9].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// clrlwi r11,r11,6
	ctx.r[11].u64 = ctx.r[11].u32 & 0x3FFFFFF;
	// addi r10,r10,-1
	ctx.r[10].s64 = ctx.r[10].s64 + -1;
	// oris r11,r11,51200
	ctx.r[11].u64 = ctx.r[11].u64 | 3355443200;
	// rlwimi r9,r10,24,3,7
	ctx.r[9].u64 = (__builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 24) & 0x1F000000) | (ctx.r[9].u64 & 0xFFFFFFFFE0FFFFFF);
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
	// stw r9,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[9].u32);
	// lwz r10,8(r26)
	ctx.r[10].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// clrlwi. r10,r10,31
	ctx.r[10].u64 = ctx.r[10].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x83059108
	if (ctx.cr0.eq) goto loc_83059108;
	// oris r11,r11,256
	ctx.r[11].u64 = ctx.r[11].u64 | 16777216;
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_83059108:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwimi r10,r11,15,12,15
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 15) & 0xF0000) | (ctx.r[10].u64 & 0xFFFFFFFFFFF0FFFF);
	// stw r10,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[10].u32);
	// lwz r11,4(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 4);
loc_8305911C:
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x83059184
	if (ctx.cr6.eq) goto loc_83059184;
	// lwz r10,16(r11)
	ctx.r[10].u64 = Load32(memory, ctx.r[11].u32 + 16);
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x8305913c
	if (ctx.cr6.eq) goto loc_8305913C;
	// lwz r10,0(r11)
	ctx.r[10].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// rlwinm. r10,r10,0,4,6
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xE000000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x83059144
	if (!ctx.cr0.eq) goto loc_83059144;
loc_8305913C:
	// lwz r11,8(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 8);
	// b 0x8305911c
	goto loc_8305911C;
loc_83059144:
	// lwz r10,8(r26)
	ctx.r[10].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm. r10,r10,27,31,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305915c
	if (ctx.cr0.eq) goto loc_8305915C;
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// ori r10,r10,32768
	ctx.r[10].u64 = ctx.r[10].u64 | 32768;
	// stw r10,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[10].u32);
loc_8305915C:
	// lwz r10,0(r11)
	ctx.r[10].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// rlwinm. r10,r10,0,27,27
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x10;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x83059174
	if (ctx.cr0.eq) goto loc_83059174;
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// ori r10,r10,64
	ctx.r[10].u64 = ctx.r[10].u64 | 64;
	// stw r10,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[10].u32);
loc_83059174:
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwimi r10,r11,15,26,31
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 15) & 0x3F) | (ctx.r[10].u64 & 0xFFFFFFFFFFFFFFC0);
	// stw r10,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[10].u32);
loc_83059184:
	// lwz r11,8(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// lis r10,8
	ctx.r[10].s64 = 524288;
	// mr r30,r29
	ctx.r[30].u64 = ctx.r[29].u64;
	// oris r11,r11,57344
	ctx.r[11].u64 = ctx.r[11].u64 | 3758096384;
	// stw r11,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[11].u32);
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm r11,r11,0,10,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x380000;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// blt cr6,0x83059200
	if (ctx.cr6.lt) goto loc_83059200;
	// addi r7,r1,80
	ctx.r[7].s64 = ctx.r[1].s64 + 80;
	// stw r29,80(r1)
	Store32(memory, ctx.r[1].u32 + 80, ctx.r[29].u32);
	// li r6,0
	ctx.r[6].s64 = 0;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x83054648
	ctx.lr = 0x830591C4;
	CallLower(0x83054648u, ctx, memory);
	// lwz r11,80(r1)
	ctx.r[11].u64 = Load32(memory, ctx.r[1].u32 + 80);
	// lwz r10,4(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// mr r30,r3
	ctx.r[30].u64 = ctx.r[3].u64;
	// rlwinm r8,r11,31,1,31
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 31) & 0x7FFFFFFF;
	// lwz r9,8(r31)
	ctx.r[9].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// rlwimi r10,r11,26,5,5
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 26) & 0x4000000) | (ctx.r[10].u64 & 0xFFFFFFFFFBFFFFFF);
	// rlwimi r9,r11,21,0,0
	ctx.r[9].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 21) & 0x80000000) | (ctx.r[9].u64 & 0xFFFFFFFF7FFFFFFF);
	// rlwinm r7,r11,21,11,31
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 21) & 0x1FFFFF;
	// stw r10,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[10].u32);
	// stb r8,5(r31)
	Store8(memory, ctx.r[31].u32 + 5, ctx.r[8].u8);
	// lwz r8,4(r31)
	ctx.r[8].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// stw r9,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[9].u32);
	// rlwimi r8,r11,22,0,0
	ctx.r[8].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 22) & 0x80000000) | (ctx.r[8].u64 & 0xFFFFFFFF7FFFFFFF);
	// stb r7,9(r31)
	Store8(memory, ctx.r[31].u32 + 9, ctx.r[7].u8);
	// stw r8,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[8].u32);
loc_83059200:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// lis r10,16
	ctx.r[10].s64 = 1048576;
	// rlwinm r11,r11,0,10,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x380000;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// blt cr6,0x83059284
	if (ctx.cr6.lt) goto loc_83059284;
	// addi r7,r1,80
	ctx.r[7].s64 = ctx.r[1].s64 + 80;
	// stw r29,80(r1)
	Store32(memory, ctx.r[1].u32 + 80, ctx.r[29].u32);
	// mr r6,r30
	ctx.r[6].u64 = ctx.r[30].u64;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// li r4,1
	ctx.r[4].s64 = 1;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x83054648
	ctx.lr = 0x83059230;
	CallLower(0x83054648u, ctx, memory);
	// lwz r11,80(r1)
	ctx.r[11].u64 = Load32(memory, ctx.r[1].u32 + 80);
	// lwz r10,4(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// lwz r9,8(r31)
	ctx.r[9].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// rlwinm r8,r11,31,1,31
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 31) & 0x7FFFFFFF;
	// rlwimi r10,r11,25,6,6
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 25) & 0x2000000) | (ctx.r[10].u64 & 0xFFFFFFFFFDFFFFFF);
	// rlwimi r9,r11,20,1,1
	ctx.r[9].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 20) & 0x40000000) | (ctx.r[9].u64 & 0xFFFFFFFFBFFFFFFF);
	// rlwinm r7,r11,21,11,31
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 21) & 0x1FFFFF;
	// stw r10,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[10].u32);
	// stw r9,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[9].u32);
	// stb r8,6(r31)
	Store8(memory, ctx.r[31].u32 + 6, ctx.r[8].u8);
	// stb r7,10(r31)
	Store8(memory, ctx.r[31].u32 + 10, ctx.r[7].u8);
	// beq 0x83059280
	if (ctx.cr0.eq) goto loc_83059280;
	// lwz r10,4(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x83059278
	if (ctx.cr6.eq) goto loc_83059278;
	// rlwimi r10,r11,21,1,1
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 21) & 0x40000000) | (ctx.r[10].u64 & 0xFFFFFFFFBFFFFFFF);
	// b 0x8305927c
	goto loc_8305927C;
loc_83059278:
	// rlwimi r10,r11,22,0,0
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 22) & 0x80000000) | (ctx.r[10].u64 & 0xFFFFFFFF7FFFFFFF);
loc_8305927C:
	// stw r10,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[10].u32);
loc_83059280:
	// add r30,r3,r30
	ctx.r[30].u64 = ctx.r[3].u64 + ctx.r[30].u64;
loc_83059284:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// lis r10,24
	ctx.r[10].s64 = 1572864;
	// rlwinm r11,r11,0,10,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x380000;
	// cmplw cr6,r11,r10
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[10].u32, ctx.xer);
	// blt cr6,0x83059304
	if (ctx.cr6.lt) goto loc_83059304;
	// addi r7,r1,80
	ctx.r[7].s64 = ctx.r[1].s64 + 80;
	// stw r29,80(r1)
	Store32(memory, ctx.r[1].u32 + 80, ctx.r[29].u32);
	// mr r6,r30
	ctx.r[6].u64 = ctx.r[30].u64;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// li r4,2
	ctx.r[4].s64 = 2;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x83054648
	ctx.lr = 0x830592B4;
	CallLower(0x83054648u, ctx, memory);
	// lwz r11,80(r1)
	ctx.r[11].u64 = Load32(memory, ctx.r[1].u32 + 80);
	// lwz r10,4(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// lwz r9,8(r31)
	ctx.r[9].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// rlwinm r8,r11,31,1,31
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 31) & 0x7FFFFFFF;
	// rlwimi r10,r11,24,7,7
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 24) & 0x1000000) | (ctx.r[10].u64 & 0xFFFFFFFFFEFFFFFF);
	// rlwimi r9,r11,19,2,2
	ctx.r[9].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 19) & 0x20000000) | (ctx.r[9].u64 & 0xFFFFFFFFDFFFFFFF);
	// rlwinm r7,r11,21,11,31
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 21) & 0x1FFFFF;
	// stw r10,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[10].u32);
	// stw r9,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[9].u32);
	// stb r8,7(r31)
	Store8(memory, ctx.r[31].u32 + 7, ctx.r[8].u8);
	// stb r7,11(r31)
	Store8(memory, ctx.r[31].u32 + 11, ctx.r[7].u8);
	// beq 0x83059304
	if (ctx.cr0.eq) goto loc_83059304;
	// lwz r10,4(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x830592fc
	if (ctx.cr6.eq) goto loc_830592FC;
	// rlwimi r10,r11,21,1,1
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 21) & 0x40000000) | (ctx.r[10].u64 & 0xFFFFFFFFBFFFFFFF);
	// b 0x83059300
	goto loc_83059300;
loc_830592FC:
	// rlwimi r10,r11,22,0,0
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 22) & 0x80000000) | (ctx.r[10].u64 & 0xFFFFFFFF7FFFFFFF);
loc_83059300:
	// stw r10,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[10].u32);
loc_83059304:
	// lwz r11,4(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// rlwinm. r10,r11,0,2,2
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x20000000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x83059334
	if (ctx.cr0.eq) goto loc_83059334;
	// rlwinm. r10,r11,0,0,0
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x80000000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x83059334
	if (!ctx.cr0.eq) goto loc_83059334;
	// rlwinm. r11,r11,0,1,1
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x83059334
	if (!ctx.cr0.eq) goto loc_83059334;
	// rlwinm r11,r26,0,0,19
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 0) & 0xFFFFF000;
	// li r4,4800
	ctx.r[4].s64 = 4800;
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lwz r3,148(r11)
	ctx.r[3].u64 = Load32(memory, ctx.r[11].u32 + 148);
	// bl 0x82f99f48
	ctx.lr = 0x83059334;
	CallDiagnostic(ctx, memory, diagnostic);
loc_83059334:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm r11,r11,25,25,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 25) & 0x7F;
	// addi r11,r11,-21
	ctx.r[11].s64 = ctx.r[11].s64 + -21;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 3, ctx.xer);
	// bgt cr6,0x8305972c
	if (ctx.cr6.gt) goto loc_8305972C;
	// lbz r11,5(r31)
	ctx.r[11].u64 = Load8(memory, ctx.r[31].u32 + 5);
	// lbz r10,6(r31)
	ctx.r[10].u64 = Load8(memory, ctx.r[31].u32 + 6);
	// clrlwi r11,r11,30
	ctx.r[11].u64 = ctx.r[11].u32 & 0x3;
	// clrlwi r10,r10,30
	ctx.r[10].u64 = ctx.r[10].u32 & 0x3;
	// addi r9,r11,-2
	ctx.r[9].s64 = ctx.r[11].s64 + -2;
	// addi r8,r11,-1
	ctx.r[8].s64 = ctx.r[11].s64 + -1;
	// addi r7,r10,-2
	ctx.r[7].s64 = ctx.r[10].s64 + -2;
	// addi r6,r10,-1
	ctx.r[6].s64 = ctx.r[10].s64 + -1;
	// rlwimi r8,r9,2,28,29
	ctx.r[8].u64 = (__builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 2) & 0xC) | (ctx.r[8].u64 & 0xFFFFFFFFFFFFFFF3);
	// rlwimi r6,r7,2,28,29
	ctx.r[6].u64 = (__builtin_rotateleft64(ctx.r[7].u32 | (ctx.r[7].u64 << 32), 2) & 0xC) | (ctx.r[6].u64 & 0xFFFFFFFFFFFFFFF3);
	// addi r9,r11,1
	ctx.r[9].s64 = ctx.r[11].s64 + 1;
	// addi r7,r10,1
	ctx.r[7].s64 = ctx.r[10].s64 + 1;
	// rlwinm r8,r8,2,26,29
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 2) & 0x3C;
	// rlwinm r9,r9,6,24,25
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 6) & 0xC0;
	// rlwinm r6,r6,2,26,29
	ctx.r[6].u64 = __builtin_rotateleft64(ctx.r[6].u32 | (ctx.r[6].u64 << 32), 2) & 0x3C;
	// rlwinm r7,r7,6,24,25
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[7].u32 | (ctx.r[7].u64 << 32), 6) & 0xC0;
	// or r9,r8,r9
	ctx.r[9].u64 = ctx.r[8].u64 | ctx.r[9].u64;
	// or r8,r6,r7
	ctx.r[8].u64 = ctx.r[6].u64 | ctx.r[7].u64;
	// or r11,r9,r11
	ctx.r[11].u64 = ctx.r[9].u64 | ctx.r[11].u64;
	// or r10,r8,r10
	ctx.r[10].u64 = ctx.r[8].u64 | ctx.r[10].u64;
	// stb r11,5(r31)
	Store8(memory, ctx.r[31].u32 + 5, ctx.r[11].u8);
	// stb r10,6(r31)
	Store8(memory, ctx.r[31].u32 + 6, ctx.r[10].u8);
	// b 0x8305972c
	goto loc_8305972C;
loc_830593A4:
	// clrlwi. r27,r28,24
	ctx.r[27].u64 = ctx.r[28].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[27].s32, 0, ctx.xer);
	// bne 0x830593bc
	if (!ctx.cr0.eq) goto loc_830593BC;
	// lwz r11,8(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// li r10,113
	ctx.r[10].s64 = 113;
	// rlwimi r11,r10,25,0,7
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 25) & 0xFF000000) | (ctx.r[11].u64 & 0xFFFFFFFF00FFFFFF);
	// b 0x83059404
	goto loc_83059404;
loc_830593BC:
	// rlwinm r11,r26,0,0,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 0) & 0xFFFFFFFE;
	// lwz r11,32(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 32);
	// clrlwi. r10,r11,31
	ctx.r[10].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x830593d4
	if (ctx.cr0.eq) goto loc_830593D4;
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
	// b 0x830593dc
	goto loc_830593DC;
loc_830593D4:
	// rlwinm r11,r11,0,0,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	// addi r11,r11,-36
	ctx.r[11].s64 = ctx.r[11].s64 + -36;
loc_830593DC:
	// lwz r11,8(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 8);
	// rlwinm. r11,r11,13,29,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 13) & 0x7;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x830593f4
	if (!ctx.cr0.eq) goto loc_830593F4;
	// lwz r10,8(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// oris r10,r10,32768
	ctx.r[10].u64 = ctx.r[10].u64 | 2147483648;
	// stw r10,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[10].u32);
loc_830593F4:
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 1, ctx.xer);
	// bne cr6,0x83059408
	if (!ctx.cr6.eq) goto loc_83059408;
	// lwz r11,8(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// oris r11,r11,16384
	ctx.r[11].u64 = ctx.r[11].u64 | 1073741824;
loc_83059404:
	// stw r11,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[11].u32);
loc_83059408:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwinm r11,r11,25,7,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 25) & 0x1FFFFFF;
	// addi r11,r11,-31
	ctx.r[11].s64 = ctx.r[11].s64 + -31;
	// rlwimi r10,r11,26,0,5
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 26) & 0xFC000000) | (ctx.r[10].u64 & 0xFFFFFFFF03FFFFFF);
	// stw r10,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[10].u32);
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// clrlwi. r11,r11,31
	ctx.r[11].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x83059438
	if (ctx.cr0.eq) goto loc_83059438;
	// rotlwi r11,r10,0
	ctx.r[11].u64 = __builtin_rotateleft32(ctx.r[10].u32, 0);
	// oris r11,r11,512
	ctx.r[11].u64 = ctx.r[11].u64 | 33554432;
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_83059438:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwinm r11,r11,19,0,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 19) & 0xFFF80000;
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
	// rlwimi r11,r10,0,12,7
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFFFF0FFFFF) | (ctx.r[11].u64 & 0xF00000);
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
	// lwz r10,4(r26)
	ctx.r[10].u64 = Load32(memory, ctx.r[26].u32 + 4);
loc_83059454:
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq cr6,0x830594cc
	if (ctx.cr6.eq) goto loc_830594CC;
	// lwz r9,16(r10)
	ctx.r[9].u64 = Load32(memory, ctx.r[10].u32 + 16);
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// beq cr6,0x83059474
	if (ctx.cr6.eq) goto loc_83059474;
	// lwz r9,0(r10)
	ctx.r[9].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// rlwinm. r9,r9,0,4,6
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0xE000000;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// bne 0x8305947c
	if (!ctx.cr0.eq) goto loc_8305947C;
loc_83059474:
	// lwz r10,8(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 8);
	// b 0x83059454
	goto loc_83059454;
loc_8305947C:
	// lwz r9,8(r26)
	ctx.r[9].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm. r9,r9,27,31,31
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x830594a4
	if (ctx.cr0.eq) goto loc_830594A4;
	// ori r11,r11,32768
	ctx.r[11].u64 = ctx.r[11].u64 | 32768;
	// mr r9,r11
	ctx.r[9].u64 = ctx.r[11].u64;
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
	// lwz r11,0(r10)
	ctx.r[11].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// rlwimi r9,r11,15,26,31
	ctx.r[9].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 15) & 0x3F) | (ctx.r[9].u64 & 0xFFFFFFFFFFFFFFC0);
	// stw r9,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[9].u32);
	// b 0x83059528
	goto loc_83059528;
loc_830594A4:
	// lwz r9,0(r10)
	ctx.r[9].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// rlwinm. r9,r9,0,27,27
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0x10;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x830594b8
	if (ctx.cr0.eq) goto loc_830594B8;
	// ori r11,r11,16384
	ctx.r[11].u64 = ctx.r[11].u64 | 16384;
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_830594B8:
	// lwz r11,0(r10)
	ctx.r[11].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwimi r10,r11,23,18,23
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 23) & 0x3F00) | (ctx.r[10].u64 & 0xFFFFFFFFFFFFC0FF);
	// stw r10,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[10].u32);
	// b 0x83059528
	goto loc_83059528;
loc_830594CC:
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x82fac070
	ctx.lr = 0x830594D4;
	CallLower(0x82fac070u, ctx, memory);
	// clrlwi. r11,r3,24
	ctx.r[11].u64 = ctx.r[3].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x83059528
	if (ctx.cr0.eq) goto loc_83059528;
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm. r11,r11,0,10,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x380000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8305959c
	if (ctx.cr0.eq) goto loc_8305959C;
	// lwz r30,40(r26)
	ctx.r[30].u64 = Load32(memory, ctx.r[26].u32 + 40);
loc_830594EC:
	// mr r3,r30
	ctx.r[3].u64 = ctx.r[30].u64;
	// bl 0x82fac608
	ctx.lr = 0x830594F4;
	CallLower(0x82fac608u, ctx, memory);
	// clrlwi. r11,r3,24
	ctx.r[11].u64 = ctx.r[3].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x83059528
	if (ctx.cr0.eq) goto loc_83059528;
	// lwz r11,0(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 0);
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwinm r11,r11,19,20,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 19) & 0xFFF;
	// rlwinm r10,r10,0,24,17
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFFFFFFC0FF;
	// rlwinm r9,r11,16,12,15
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 16) & 0xF0000;
	// rlwinm r11,r11,0,22,27
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x3F0;
	// rlwinm r10,r10,0,12,7
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFFFF0FFFFF;
	// or r11,r9,r11
	ctx.r[11].u64 = ctx.r[9].u64 | ctx.r[11].u64;
	// rlwinm r11,r11,4,0,27
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 4) & 0xFFFFFFF0;
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
loc_83059528:
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm. r11,r11,0,10,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x380000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8305972c
	if (ctx.cr0.eq) goto loc_8305972C;
	// mr r30,r29
	ctx.r[30].u64 = ctx.r[29].u64;
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x8305955c
	if (ctx.cr6.eq) goto loc_8305955C;
	// lwz r11,8(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// rlwinm. r10,r11,0,0,0
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x80000000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x83059550
	if (!ctx.cr0.eq) goto loc_83059550;
	// li r30,1
	ctx.r[30].s64 = 1;
loc_83059550:
	// rlwinm. r11,r11,0,1,1
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x40000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x8305955c
	if (!ctx.cr0.eq) goto loc_8305955C;
	// addi r30,r30,1
	ctx.r[30].s64 = ctx.r[30].s64 + 1;
loc_8305955C:
	// addi r7,r1,80
	ctx.r[7].s64 = ctx.r[1].s64 + 80;
	// stw r29,80(r1)
	Store32(memory, ctx.r[1].u32 + 80, ctx.r[29].u32);
	// mr r6,r30
	ctx.r[6].u64 = ctx.r[30].u64;
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// li r4,0
	ctx.r[4].s64 = 0;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x83054648
	ctx.lr = 0x83059578;
	CallLower(0x83054648u, ctx, memory);
	// add r6,r3,r30
	ctx.r[6].u64 = ctx.r[3].u64 + ctx.r[30].u64;
	// lwz r30,80(r1)
	ctx.r[30].u64 = Load32(memory, ctx.r[1].u32 + 80);
	// cmplwi r3,0
	ctx.cr0.compare<uint32_t>(ctx.r[3].u32, 0, ctx.xer);
	// beq 0x830595d8
	if (ctx.cr0.eq) goto loc_830595D8;
	// lwz r11,4(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// cmplwi cr6,r6,2
	ctx.cr6.compare<uint32_t>(ctx.r[6].u32, 2, ctx.xer);
	// bne cr6,0x830595d0
	if (!ctx.cr6.eq) goto loc_830595D0;
	// rlwimi r11,r30,21,1,1
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 21) & 0x40000000) | (ctx.r[11].u64 & 0xFFFFFFFFBFFFFFFF);
	// b 0x830595d4
	goto loc_830595D4;
loc_8305959C:
	// lwz r30,0(r26)
	ctx.r[30].u64 = Load32(memory, ctx.r[26].u32 + 0);
loc_830595A0:
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x830595bc
	if (ctx.cr6.eq) goto loc_830595BC;
	// lwz r11,0(r30)
	ctx.r[11].u64 = Load32(memory, ctx.r[30].u32 + 0);
	// rlwinm. r11,r11,0,4,6
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xE000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x830594ec
	if (!ctx.cr0.eq) goto loc_830594EC;
	// lwz r30,4(r30)
	ctx.r[30].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// b 0x830595a0
	goto loc_830595A0;
loc_830595BC:
	// rlwinm r11,r26,0,0,19
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 0) & 0xFFFFF000;
	// li r4,4800
	ctx.r[4].s64 = 4800;
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lwz r3,148(r11)
	ctx.r[3].u64 = Load32(memory, ctx.r[11].u32 + 148);
	// bl 0x82f99f48
	ctx.lr = 0x830595D0;
	CallDiagnostic(ctx, memory, diagnostic);
loc_830595D0:
	// rlwimi r11,r30,22,0,0
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 22) & 0x80000000) | (ctx.r[11].u64 & 0xFFFFFFFF7FFFFFFF);
loc_830595D4:
	// stw r11,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[11].u32);
loc_830595D8:
	// lwz r5,4(r31)
	ctx.r[5].u64 = Load32(memory, ctx.r[31].u32 + 4);
	// rlwinm r11,r30,21,11,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 21) & 0x1FFFFF;
	// lis r4,16
	ctx.r[4].s64 = 1048576;
	// rlwimi r5,r30,24,7,7
	ctx.r[5].u64 = (__builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 24) & 0x1000000) | (ctx.r[5].u64 & 0xFFFFFFFFFEFFFFFF);
	// stb r11,11(r31)
	Store8(memory, ctx.r[31].u32 + 11, ctx.r[11].u8);
	// stw r5,4(r31)
	Store32(memory, ctx.r[31].u32 + 4, ctx.r[5].u32);
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm r11,r11,0,10,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x380000;
	// cmplw cr6,r11,r4
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[4].u32, ctx.xer);
	// blt cr6,0x83059674
	if (ctx.cr6.lt) goto loc_83059674;
	// addi r7,r1,80
	ctx.r[7].s64 = ctx.r[1].s64 + 80;
	// stw r29,80(r1)
	Store32(memory, ctx.r[1].u32 + 80, ctx.r[29].u32);
	// mr r5,r31
	ctx.r[5].u64 = ctx.r[31].u64;
	// li r4,1
	ctx.r[4].s64 = 1;
	// mr r3,r26
	ctx.r[3].u64 = ctx.r[26].u64;
	// bl 0x83054648
	ctx.lr = 0x83059618;
	CallLower(0x83054648u, ctx, memory);
	// lwz r9,80(r1)
	ctx.r[9].u64 = Load32(memory, ctx.r[1].u32 + 80);
	// rlwinm r11,r9,21,24,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 21) & 0xFF;
	// clrlwi. r10,r11,31
	ctx.r[10].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305963c
	if (ctx.cr0.eq) goto loc_8305963C;
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwinm r8,r10,0,0,5
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFC000000;
	// addis r8,r8,1024
	ctx.r[8].s64 = ctx.r[8].s64 + 67108864;
	// rlwimi r8,r10,0,6,31
	ctx.r[8].u64 = (__builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x3FFFFFF) | (ctx.r[8].u64 & 0xFFFFFFFFFC000000);
	// stw r8,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[8].u32);
loc_8305963C:
	// rlwinm r8,r11,30,28,29
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 30) & 0xC;
	// lwz r7,8(r31)
	ctx.r[7].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// rlwinm r6,r11,30,30,31
	ctx.r[6].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 30) & 0x3;
	// rlwinm r10,r30,31,30,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 31) & 0x3;
	// or r8,r8,r6
	ctx.r[8].u64 = ctx.r[8].u64 | ctx.r[6].u64;
	// addi r10,r10,1
	ctx.r[10].s64 = ctx.r[10].s64 + 1;
	// rlwimi r7,r11,28,2,2
	ctx.r[7].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 28) & 0x20000000) | (ctx.r[7].u64 & 0xFFFFFFFFDFFFFFFF);
	// rlwinm r8,r8,2,0,29
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 2) & 0xFFFFFFFC;
	// rlwinm r10,r10,6,24,25
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 6) & 0xC0;
	// rlwinm r9,r9,31,30,31
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 31) & 0x3;
	// or r10,r8,r10
	ctx.r[10].u64 = ctx.r[8].u64 | ctx.r[10].u64;
	// stw r7,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[7].u32);
	// or r11,r10,r9
	ctx.r[11].u64 = ctx.r[10].u64 | ctx.r[9].u64;
	// b 0x83059728
	goto loc_83059728;
loc_83059674:
	// lwz r11,8(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 8);
	// rlwinm r3,r30,31,24,31
	ctx.r[3].u64 = __builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 31) & 0xFF;
	// rlwimi r11,r30,19,2,2
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[30].u32 | (ctx.r[30].u64 << 32), 19) & 0x20000000) | (ctx.r[11].u64 & 0xFFFFFFFFDFFFFFFF);
	// stw r11,8(r31)
	Store32(memory, ctx.r[31].u32 + 8, ctx.r[11].u32);
	// bl 0x83054600
	ctx.lr = 0x83059688;
	CallLower(0x83054600u, ctx, memory);
	// mr r6,r3
	ctx.r[6].u64 = ctx.r[3].u64;
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x83059700
	if (ctx.cr6.eq) goto loc_83059700;
	// rlwinm r11,r26,0,0,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 0) & 0xFFFFFFFE;
	// lwz r11,32(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 32);
	// clrlwi. r10,r11,31
	ctx.r[10].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x830596ac
	if (ctx.cr0.eq) goto loc_830596AC;
	// mr r11,r29
	ctx.r[11].u64 = ctx.r[29].u64;
	// b 0x830596b4
	goto loc_830596B4;
loc_830596AC:
	// rlwinm r11,r11,0,0,30
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFE;
	// addi r11,r11,-36
	ctx.r[11].s64 = ctx.r[11].s64 + -36;
loc_830596B4:
	// lwz r11,8(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 8);
	// rlwinm r11,r11,0,10,12
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x380000;
	// cmplw cr6,r11,r4
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, ctx.r[4].u32, ctx.xer);
	// ble cr6,0x83059700
	if (!ctx.cr6.gt) goto loc_83059700;
	// clrlwi r3,r5,24
	ctx.r[3].u64 = ctx.r[5].u32 & 0xFF;
	// bl 0x83054600
	ctx.lr = 0x830596CC;
	CallLower(0x83054600u, ctx, memory);
	// clrlwi r11,r6,30
	ctx.r[11].u64 = ctx.r[6].u32 & 0x3;
	// rlwinm r10,r6,30,2,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[6].u32 | (ctx.r[6].u64 << 32), 30) & 0x3FFFFFFF;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// addi r10,r10,-2
	ctx.r[10].s64 = ctx.r[10].s64 + -2;
	// rlwinm r9,r11,6,24,25
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 6) & 0xC0;
	// rlwinm r11,r3,30,2,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[3].u32 | (ctx.r[3].u64 << 32), 30) & 0x3FFFFFFF;
	// clrlwi r8,r3,30
	ctx.r[8].u64 = ctx.r[3].u32 & 0x3;
	// addi r11,r11,-1
	ctx.r[11].s64 = ctx.r[11].s64 + -1;
	// rlwimi r11,r10,2,28,29
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 2) & 0xC) | (ctx.r[11].u64 & 0xFFFFFFFFFFFFFFF3);
	// rlwinm r11,r11,2,26,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0x3C;
	// or r11,r11,r9
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[9].u64;
	// or r11,r11,r8
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[8].u64;
	// b 0x83059728
	goto loc_83059728;
loc_83059700:
	// clrlwi r11,r6,30
	ctx.r[11].u64 = ctx.r[6].u32 & 0x3;
	// rlwinm r10,r6,30,30,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[6].u32 | (ctx.r[6].u64 << 32), 30) & 0x3;
	// addi r9,r11,-2
	ctx.r[9].s64 = ctx.r[11].s64 + -2;
	// addi r8,r10,-1
	ctx.r[8].s64 = ctx.r[10].s64 + -1;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rlwimi r8,r9,2,28,29
	ctx.r[8].u64 = (__builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 2) & 0xC) | (ctx.r[8].u64 & 0xFFFFFFFFFFFFFFF3);
	// rlwinm r11,r11,6,24,25
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 6) & 0xC0;
	// rlwinm r9,r8,2,26,29
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 2) & 0x3C;
	// or r11,r9,r11
	ctx.r[11].u64 = ctx.r[9].u64 | ctx.r[11].u64;
	// or r11,r11,r10
	ctx.r[11].u64 = ctx.r[11].u64 | ctx.r[10].u64;
loc_83059728:
	// stb r11,7(r31)
	Store8(memory, ctx.r[31].u32 + 7, ctx.r[11].u8);
loc_8305972C:
	// clrlwi. r11,r28,24
	ctx.r[11].u64 = ctx.r[28].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x830597e0
	if (!ctx.cr0.eq) goto loc_830597E0;
	// lwz r11,8(r26)
	ctx.r[11].u64 = Load32(memory, ctx.r[26].u32 + 8);
	// rlwinm. r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x830597e0
	if (ctx.cr0.eq) goto loc_830597E0;
	// lwz r27,4(r26)
	ctx.r[27].u64 = Load32(memory, ctx.r[26].u32 + 4);
loc_83059744:
	// cmplwi cr6,r27,0
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, 0, ctx.xer);
	// beq cr6,0x8305976c
	if (ctx.cr6.eq) goto loc_8305976C;
	// lwz r11,16(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 16);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// beq cr6,0x83059764
	if (ctx.cr6.eq) goto loc_83059764;
	// lwz r11,0(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 0);
	// rlwinm. r11,r11,0,4,6
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xE000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x8305976c
	if (!ctx.cr0.eq) goto loc_8305976C;
loc_83059764:
	// lwz r27,8(r27)
	ctx.r[27].u64 = Load32(memory, ctx.r[27].u32 + 8);
	// b 0x83059744
	goto loc_83059744;
loc_8305976C:
	// lwz r11,16(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 16);
	// mr r28,r29
	ctx.r[28].u64 = ctx.r[29].u64;
	// lwz r30,0(r11)
	ctx.r[30].u64 = Load32(memory, ctx.r[11].u32 + 0);
loc_83059778:
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r[30].u32, 0, ctx.xer);
	// beq cr6,0x83059804
	if (ctx.cr6.eq) goto loc_83059804;
	// lwz r10,0(r30)
	ctx.r[10].u64 = Load32(memory, ctx.r[30].u32 + 0);
	// rlwinm. r11,r10,0,4,6
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xE000000;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x830597fc
	if (ctx.cr0.eq) goto loc_830597FC;
	// lwz r3,12(r30)
	ctx.r[3].u64 = Load32(memory, ctx.r[30].u32 + 12);
	// lwz r11,8(r3)
	ctx.r[11].u64 = Load32(memory, ctx.r[3].u32 + 8);
	// rlwinm. r9,r11,27,31,31
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x830597fc
	if (ctx.cr0.eq) goto loc_830597FC;
	// lwz r9,0(r31)
	ctx.r[9].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwinm r10,r10,15,17,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 15) & 0x7FFF;
	// clrlwi r9,r9,26
	ctx.r[9].u64 = ctx.r[9].u32 & 0x3F;
	// xor r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 ^ ctx.r[9].u64;
	// clrlwi. r10,r10,24
	ctx.r[10].u64 = ctx.r[10].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x830597fc
	if (!ctx.cr0.eq) goto loc_830597FC;
	// rlwinm r11,r11,25,25,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 25) & 0x7F;
	// cmplwi cr6,r11,120
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 120, ctx.xer);
	// beq cr6,0x830597e8
	if (ctx.cr6.eq) goto loc_830597E8;
	// cmplwi cr6,r11,121
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 121, ctx.xer);
	// beq cr6,0x830597e8
	if (ctx.cr6.eq) goto loc_830597E8;
	// cmplw cr6,r3,r26
	ctx.cr6.compare<uint32_t>(ctx.r[3].u32, ctx.r[26].u32, ctx.xer);
	// beq cr6,0x830597fc
	if (ctx.cr6.eq) goto loc_830597FC;
	// mr r4,r26
	ctx.r[4].u64 = ctx.r[26].u64;
	// bl 0x82fb71d0
	ctx.lr = 0x830597D8;
	CallLower(0x82fb71d0u, ctx, memory);
	// clrlwi. r11,r3,24
	ctx.r[11].u64 = ctx.r[3].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x830597fc
	if (ctx.cr0.eq) goto loc_830597FC;
loc_830597E0:
	// addi r1,r1,144
	ctx.r[1].s64 = ctx.r[1].s64 + 144;
	// b 0x82b7a730
	Restore26(ctx, memory);
	return;
loc_830597E8:
	// addi r11,r11,-121
	ctx.r[11].s64 = ctx.r[11].s64 + -121;
	// li r28,1
	ctx.r[28].s64 = 1;
	// cntlzw r11,r11
	ctx.r[11].u64 = ctx.r[11].u32 == 0 ? 32 : __builtin_clz(ctx.r[11].u32);
	// rlwinm r11,r11,27,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x1;
	// or r29,r11,r29
	ctx.r[29].u64 = ctx.r[11].u64 | ctx.r[29].u64;
loc_830597FC:
	// lwz r30,4(r30)
	ctx.r[30].u64 = Load32(memory, ctx.r[30].u32 + 4);
	// b 0x83059778
	goto loc_83059778;
loc_83059804:
	// clrlwi. r11,r28,24
	ctx.r[11].u64 = ctx.r[28].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x830597e0
	if (ctx.cr0.eq) goto loc_830597E0;
	// lwz r11,0(r31)
	ctx.r[11].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// clrlwi. r10,r29,24
	ctx.r[10].u64 = ctx.r[29].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// ori r11,r11,16384
	ctx.r[11].u64 = ctx.r[11].u64 | 16384;
	// stw r11,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[11].u32);
	// beq 0x830597e0
	if (ctx.cr0.eq) goto loc_830597E0;
	// lwz r11,16(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 16);
	// lwz r9,0(r11)
	ctx.r[9].u64 = Load32(memory, ctx.r[11].u32 + 0);
loc_83059828:
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r[9].u32, 0, ctx.xer);
	// beq cr6,0x830597e0
	if (ctx.cr6.eq) goto loc_830597E0;
	// lwz r11,0(r9)
	ctx.r[11].u64 = Load32(memory, ctx.r[9].u32 + 0);
	// rlwinm. r10,r11,0,4,6
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xE000000;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x83059888
	if (ctx.cr0.eq) goto loc_83059888;
	// lwz r10,12(r9)
	ctx.r[10].u64 = Load32(memory, ctx.r[9].u32 + 12);
	// lwz r10,8(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 8);
	// rlwinm r10,r10,0,18,24
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x3F80;
	// cmplwi cr6,r10,15488
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 15488, ctx.xer);
	// bne cr6,0x83059888
	if (!ctx.cr6.eq) goto loc_83059888;
	// lwz r10,0(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// rlwinm r11,r11,19,20,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 19) & 0xFFF;
	// clrlwi r8,r10,26
	ctx.r[8].u64 = ctx.r[10].u32 & 0x3F;
	// rlwinm r7,r11,28,4,31
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 28) & 0xFFFFFFF;
	// cmplw cr6,r7,r8
	ctx.cr6.compare<uint32_t>(ctx.r[7].u32, ctx.r[8].u32, ctx.xer);
	// bne cr6,0x83059888
	if (!ctx.cr6.eq) goto loc_83059888;
	// clrlwi r11,r11,28
	ctx.r[11].u64 = ctx.r[11].u32 & 0xF;
	// rlwinm r8,r11,20,0,11
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 20) & 0xFFF00000;
	// rlwinm r7,r11,16,0,15
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 16) & 0xFFFF0000;
	// or r11,r8,r10
	ctx.r[11].u64 = ctx.r[8].u64 | ctx.r[10].u64;
	// rlwimi r11,r10,0,12,7
	ctx.r[11].u64 = (__builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0xFFFFFFFFFF0FFFFF) | (ctx.r[11].u64 & 0xF00000);
	// or r10,r7,r11
	ctx.r[10].u64 = ctx.r[7].u64 | ctx.r[11].u64;
	// rlwimi r10,r11,0,16,11
	ctx.r[10].u64 = (__builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0xFFFFFFFFFFF0FFFF) | (ctx.r[10].u64 & 0xF0000);
	// stw r10,0(r31)
	Store32(memory, ctx.r[31].u32 + 0, ctx.r[10].u32);
loc_83059888:
	// lwz r9,4(r9)
	ctx.r[9].u64 = Load32(memory, ctx.r[9].u32 + 4);
	// b 0x83059828
	goto loc_83059828;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    DiagnosticServices& diagnostic, Registers& registers)
{
    if (entry != 0x83058fc8u) return false;
    auto context = ToContext(registers);
    Body(context, memory, diagnostic);
    FromContext(context, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_descriptor_search_caller
