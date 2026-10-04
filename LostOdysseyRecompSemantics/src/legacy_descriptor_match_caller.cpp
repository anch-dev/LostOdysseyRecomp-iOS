#include "lo_semantics/legacy_descriptor_match_caller.h"

#include "lo_semantics/legacy_descriptor_record_routes.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_match_caller
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
union FpReg
{
    std::uint64_t u64 = 0;
    std::int64_t s64;
    double f64;
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
    FpReg f0{}, f1{}, f2{}, f3{}, f4{}, f12{}, f13{}, f31{};
    std::uint64_t lr = 0, ctr = 0;
    std::uint32_t fp_control = 0;
    Xer xer{};
    Condition cr0{}, cr6{};
};
using Integer = crt_stream_operations::Registers;
void ToInteger(const Context& c, Integer& s)
{
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) s.r[i] = c.r[i].u64;
    s.sp = c.r[1].u64; s.lr = c.lr; s.ctr = c.ctr;
    s.xer_ca = c.xer.ca; s.xer_so = c.xer.so;
    s.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
}
void FromInteger(const Integer& s, Context& c)
{
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) c.r[i].u64 = s.r[i];
    c.r[1].u64 = s.sp; c.lr = s.lr; c.ctr = s.ctr;
    c.xer = {s.xer_ca, s.xer_so};
    c.cr0 = {s.cr0.lt, s.cr0.gt, s.cr0.eq, s.cr0.so};
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, s.cr6.so};
}
Context ToContext(const Registers& s)
{
    Context c{};
    FromInteger(s.numeric.classifier.integer, c);
    c.f0.u64 = s.numeric.classifier.f0_bits;
    c.f1.u64 = s.numeric.classifier.f1_bits;
    c.f2.u64 = s.f2_bits;
    c.f3.u64 = s.f3_bits; c.f4.u64 = s.f4_bits;
    c.f12.u64 = s.f12_bits; c.f13.u64 = s.f13_bits;
    c.f31.u64 = s.numeric.f31_bits;
    c.fp_control = s.numeric.classifier.cached_fp_control;
    return c;
}
void FromContext(const Context& c, Registers& s)
{
    ToInteger(c, s.numeric.classifier.integer);
    s.numeric.classifier.f0_bits = c.f0.u64;
    s.numeric.classifier.f1_bits = c.f1.u64;
    s.f2_bits = c.f2.u64;
    s.f3_bits = c.f3.u64; s.f4_bits = c.f4.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.numeric.f31_bits = c.f31.u64;
    s.numeric.classifier.cached_fp_control = c.fp_control;
}
void DisableFlush(Context& c,
    legacy_descriptor_numeric_match_routes::Services& services)
{
    if (c.fp_control & 0x8040u)
    {
        c.fp_control &= ~0x8040u;
        services.SetHostFpControl(c.fp_control);
    }
}
void CompareFp(Condition& cr, std::uint64_t left_bits,
    std::uint64_t right_bits)
{
    constexpr std::uint64_t Exponent = 0x7ff0000000000000ull;
    constexpr std::uint64_t Fraction = 0x000fffffffffffffull;
    constexpr std::uint64_t Quiet = 0x0008000000000000ull;
    const bool left_nan = (left_bits & Exponent) == Exponent &&
        (left_bits & Fraction) != 0u;
    const bool right_nan = (right_bits & Exponent) == Exponent &&
        (right_bits & Fraction) != 0u;
    if (left_nan || right_nan)
    {
        if ((left_nan && (left_bits & Quiet) == 0u) ||
            (right_nan && (right_bits & Quiet) == 0u))
        {
            volatile double left = std::bit_cast<double>(left_bits);
            volatile double right = std::bit_cast<double>(right_bits);
            volatile bool probe = left == right;
            (void)probe;
        }
        cr = {0u, 0u, 0u, 1u};
        return;
    }
    const double left = std::bit_cast<double>(left_bits);
    const double right = std::bit_cast<double>(right_bits);
    cr = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), 0u};
}
std::uint32_t Load32(GuestMemory& m, std::uint64_t a)
{ return m.ReadU32(Address(a)); }
std::uint64_t Load64(GuestMemory& m, std::uint64_t a)
{ return ReadU64(m, Address(a)); }
void Store32(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ m.WriteU32(Address(a), Address(v)); }
void Store64(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ WriteU64(m, Address(a), v); }
void Save17(Context& c, GuestMemory& m)
{
    for (unsigned i = 17u; i <= 31u; ++i)
        Store64(m, c.r[1].u64 - (33u - i) * 8u, c.r[i].u64);
    Store32(m, c.r[1].u64 - 8u, c.r[12].u64);
}
void Restore17(Context& c, GuestMemory& m)
{
    for (unsigned i = 17u; i <= 31u; ++i)
        c.r[i].u64 = Load64(m, c.r[1].u64 - (33u - i) * 8u);
    c.r[12].u64 = Load32(m, c.r[1].u64 - 8u);
    c.lr = c.r[12].u64;
}
void CallSelector(Context& c, GuestMemory& m)
{
    Integer s{}; ToInteger(c, s);
    (void)legacy_descriptor_record_routes::Apply(0x82fac238u, m, s);
    FromInteger(s, c);
}
void CallNumeric(GuestAddress entry, Context& c, GuestMemory& m,
    Dependencies d)
{
    Registers s{}; FromContext(c, s);
    (void)legacy_descriptor_numeric_match_routes::Apply(entry, m, d.numeric, s);
    auto next = ToContext(s);
    c = next;
}
void CallScalar(Context& c, GuestMemory& m, Dependencies d)
{
    legacy_descriptor_scalar_intern_callers::Registers s{};
    ToInteger(c, s.integer);
    s.f0_bits = c.f0.u64; s.f1_bits = c.f1.u64;
    s.f2_bits = c.f2.u64; s.f3_bits = c.f3.u64; s.f4_bits = c.f4.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.cached_fp_control = c.fp_control;
    (void)legacy_descriptor_scalar_intern_callers::Apply(0x8305a7c0u,
        m, d.scalar, s);
    FromInteger(s.integer, c);
    c.f0.u64 = s.f0_bits; c.f1.u64 = s.f1_bits;
    c.f2.u64 = s.f2_bits; c.f3.u64 = s.f3_bits; c.f4.u64 = s.f4_bits;
    c.f12.u64 = s.f12_bits; c.f13.u64 = s.f13_bits;
    c.fp_control = s.cached_fp_control;
}
void CallDiagnostic(Context& c, GuestMemory& m, Dependencies d)
{
    c.r[12].u64 = c.lr;
    Store32(m, c.r[1].u64 - 8u, c.r[12].u64);
    for (unsigned i = 5u; i <= 10u; ++i)
        Store64(m, c.r[1].u64 + 32u + (i - 5u) * 8u, c.r[i].u64);
    const auto prior = c.r[1].u64;
    Store32(m, prior - 96u, prior);
    c.r[1].u64 -= 96u;
    c.r[11].u64 = c.r[1].u64 + 80u;
    c.r[10].u64 = c.r[1].u64 + 128u;
    Store32(m, c.r[11].u64, c.r[10].u64);
    c.r[5].u64 = Load32(m, c.r[1].u64 + 80u);
    c.lr = 0x82f99f80u;
    Registers s{}; FromContext(c, s);
    d.diagnostic.Call(0x82f99d98u, m, s);
    c = ToContext(s);
}

void Body(Context& ctx, GuestMemory& memory, Dependencies dependencies)
{
	Reg temp{};
	// mflr r12
	ctx.r[12].u64 = ctx.lr;
	// bl 0x82b7a6bc
	ctx.lr = 0x8305A870;
	Save17(ctx, memory);
	// stfd f31,-136(r1)
	DisableFlush(ctx, dependencies.numeric);
	Store64(memory, ctx.r[1].u32 + -136, ctx.f31.u64);
	// stwu r1,-224(r1)
	temp.u64 = ctx.r[1].u64 + uint64_t(-224);
	Store32(memory, temp.u32, ctx.r[1].u32);
	ctx.r[1].u64 = temp.u64;
	// mr r22,r3
	ctx.r[22].u64 = ctx.r[3].u64;
	// mr r21,r10
	ctx.r[21].u64 = ctx.r[10].u64;
	// rlwinm r10,r4,1,0,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[4].u32 | (ctx.r[4].u64 << 32), 1) & 0xFFFFFFFE;
	// mr r19,r5
	ctx.r[19].u64 = ctx.r[5].u64;
	// mr r18,r6
	ctx.r[18].u64 = ctx.r[6].u64;
	// lwz r11,0(r22)
	ctx.r[11].u64 = Load32(memory, ctx.r[22].u32 + 0);
	// mr r17,r7
	ctx.r[17].u64 = ctx.r[7].u64;
	// lwz r27,12(r22)
	ctx.r[27].u64 = Load32(memory, ctx.r[22].u32 + 12);
	// mr r20,r8
	ctx.r[20].u64 = ctx.r[8].u64;
	// clrlwi r28,r11,27
	ctx.r[28].u64 = ctx.r[11].u32 & 0x1F;
	// rlwinm r11,r11,27,24,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0xFF;
	// mr r23,r9
	ctx.r[23].u64 = ctx.r[9].u64;
	// li r24,0
	ctx.r[24].s64 = 0;
	// li r25,0
	ctx.r[25].s64 = 0;
	// srw r11,r11,r10
	ctx.r[11].u64 = ctx.r[10].u8 & 0x20 ? 0 : (ctx.r[11].u32 >> (ctx.r[10].u8 & 0x3F));
	// clrlwi r26,r11,30
	ctx.r[26].u64 = ctx.r[11].u32 & 0x3;
	// and. r11,r28,r21
	ctx.r[11].u64 = ctx.r[28].u64 & ctx.r[21].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// lis r11,-32256
	ctx.r[11].s64 = -2113929216;
	// lfd f31,4072(r11)
	ctx.f31.u64 = Load64(memory, ctx.r[11].u32 + 4072);
loc_8305A8C8:
	// lwz r10,8(r27)
	ctx.r[10].u64 = Load32(memory, ctx.r[27].u32 + 8);
	// rlwinm r11,r10,25,25,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 25) & 0x7F;
	// cmpwi cr6,r11,3
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 3, ctx.xer);
	// beq cr6,0x8305ace4
	if (ctx.cr6.eq) goto loc_8305ACE4;
	// cmpwi cr6,r11,13
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 13, ctx.xer);
	// beq cr6,0x8305ac14
	if (ctx.cr6.eq) goto loc_8305AC14;
	// cmpwi cr6,r11,102
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 102, ctx.xer);
	// beq cr6,0x8305ab0c
	if (ctx.cr6.eq) goto loc_8305AB0C;
	// cmpwi cr6,r11,109
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 109, ctx.xer);
	// beq cr6,0x8305aa44
	if (ctx.cr6.eq) goto loc_8305AA44;
	// cmpwi cr6,r11,111
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 111, ctx.xer);
	// bne cr6,0x8305add0
	if (!ctx.cr6.eq) goto loc_8305ADD0;
	// cmplwi cr6,r20,0
	ctx.cr6.compare<uint32_t>(ctx.r[20].u32, 0, ctx.xer);
	// bne cr6,0x8305aebc
	if (!ctx.cr6.eq) goto loc_8305AEBC;
	// lwz r10,0(r27)
	ctx.r[10].u64 = Load32(memory, ctx.r[27].u32 + 0);
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq 0x8305a91c
	if (ctx.cr0.eq) goto loc_8305A91C;
	// lwz r11,4(r10)
	ctx.r[11].u64 = Load32(memory, ctx.r[10].u32 + 4);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 0, ctx.xer);
	// li r11,1
	ctx.r[11].s64 = 1;
	// bne cr6,0x8305a920
	if (!ctx.cr6.eq) goto loc_8305A920;
loc_8305A91C:
	// li r11,0
	ctx.r[11].s64 = 0;
loc_8305A920:
	// clrlwi. r11,r11,24
	ctx.r[11].u64 = ctx.r[11].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// cmplwi r10,0
	ctx.cr0.compare<uint32_t>(ctx.r[10].u32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// lwz r6,12(r10)
	ctx.r[6].u64 = Load32(memory, ctx.r[10].u32 + 12);
	// cmplw cr6,r27,r6
	ctx.cr6.compare<uint32_t>(ctx.r[27].u32, ctx.r[6].u32, ctx.xer);
	// beq cr6,0x8305aebc
	if (ctx.cr6.eq) goto loc_8305AEBC;
	// lwz r11,24(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 24);
	// lwz r9,24(r6)
	ctx.r[9].u64 = Load32(memory, ctx.r[6].u32 + 24);
	// lwz r8,48(r11)
	ctx.r[8].u64 = Load32(memory, ctx.r[11].u32 + 48);
	// lwz r7,40(r9)
	ctx.r[7].u64 = Load32(memory, ctx.r[9].u32 + 40);
	// clrlwi r11,r8,13
	ctx.r[11].u64 = ctx.r[8].u32 & 0x7FFFF;
	// clrlwi r5,r11,27
	ctx.r[5].u64 = ctx.r[11].u32 & 0x1F;
	// rlwinm r11,r11,27,5,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0x7FFFFFF;
	// addi r11,r11,1
	ctx.r[11].s64 = ctx.r[11].s64 + 1;
	// rlwinm r11,r11,2,0,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r11,r11,r7
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + ctx.r[7].u32);
	// li r7,1
	ctx.r[7].s64 = 1;
	// slw r7,r7,r5
	ctx.r[7].u64 = ctx.r[5].u8 & 0x20 ? 0 : (ctx.r[7].u32 << (ctx.r[5].u8 & 0x3F));
	// and. r11,r11,r7
	ctx.r[11].u64 = ctx.r[11].u64 & ctx.r[7].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// lwz r7,0(r10)
	ctx.r[7].u64 = Load32(memory, ctx.r[10].u32 + 0);
	// clrlwi r11,r7,27
	ctx.r[11].u64 = ctx.r[7].u32 & 0x1F;
	// rlwinm. r10,r11,0,27,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x18;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305a994
	if (ctx.cr0.eq) goto loc_8305A994;
	// rlwinm. r10,r28,0,27,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 0) & 0x18;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305a994
	if (ctx.cr0.eq) goto loc_8305A994;
loc_8305A98C:
	// li r10,0
	ctx.r[10].s64 = 0;
	// b 0x8305a9bc
	goto loc_8305A9BC;
loc_8305A994:
	// rlwinm. r10,r11,0,30,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305a9a4
	if (ctx.cr0.eq) goto loc_8305A9A4;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305a98c
	if (!ctx.cr0.eq) goto loc_8305A98C;
loc_8305A9A4:
	// rlwinm. r10,r11,0,29,29
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305a9b8
	if (ctx.cr0.eq) goto loc_8305A9B8;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// li r10,0
	ctx.r[10].s64 = 0;
	// bne 0x8305a9bc
	if (!ctx.cr0.eq) goto loc_8305A9BC;
loc_8305A9B8:
	// li r10,1
	ctx.r[10].s64 = 1;
loc_8305A9BC:
	// clrlwi. r10,r10,24
	ctx.r[10].u64 = ctx.r[10].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x8305a9e4
	if (ctx.cr6.eq) goto loc_8305A9E4;
	// lwz r10,24(r23)
	ctx.r[10].u64 = Load32(memory, ctx.r[23].u32 + 24);
	// lwz r9,76(r9)
	ctx.r[9].u64 = Load32(memory, ctx.r[9].u32 + 76);
	// lwz r10,76(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 76);
	// xor r10,r10,r9
	ctx.r[10].u64 = ctx.r[10].u64 ^ ctx.r[9].u64;
	// clrlwi. r10,r10,13
	ctx.r[10].u64 = ctx.r[10].u32 & 0x7FFFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
loc_8305A9E4:
	// rlwinm. r10,r8,11,31,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 11) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// rlwinm. r10,r8,13,31,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 13) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// rlwinm. r9,r11,0,29,29
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// rlwinm r26,r7,27,30,31
	ctx.r[26].u64 = __builtin_rotateleft64(ctx.r[7].u32 | (ctx.r[7].u64 << 32), 27) & 0x3;
	// mr r10,r28
	ctx.r[10].u64 = ctx.r[28].u64;
	// beq 0x8305aa10
	if (ctx.cr0.eq) goto loc_8305AA10;
	// rlwinm. r9,r28,0,30,30
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305aa10
	if (ctx.cr0.eq) goto loc_8305AA10;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
loc_8305AA10:
	// and r9,r11,r28
	ctx.r[9].u64 = ctx.r[11].u64 & ctx.r[28].u64;
	// rlwinm. r9,r9,0,29,29
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305aa24
	if (ctx.cr0.eq) goto loc_8305AA24;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
	// addi r10,r28,-4
	ctx.r[10].s64 = ctx.r[28].s64 + -4;
loc_8305AA24:
	// clrlwi. r9,r11,31
	ctx.r[9].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305aa38
	if (ctx.cr0.eq) goto loc_8305AA38;
	// rlwinm. r9,r10,0,30,30
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305aa38
	if (ctx.cr0.eq) goto loc_8305AA38;
	// addi r10,r10,-2
	ctx.r[10].s64 = ctx.r[10].s64 + -2;
loc_8305AA38:
	// or r28,r10,r11
	ctx.r[28].u64 = ctx.r[10].u64 | ctx.r[11].u64;
	// mr r27,r6
	ctx.r[27].u64 = ctx.r[6].u64;
	// b 0x8305adc4
	goto loc_8305ADC4;
loc_8305AA44:
	// addi r11,r26,10
	ctx.r[11].s64 = ctx.r[26].s64 + 10;
	// rlwinm r11,r11,2,0,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// lwzx r8,r11,r27
	ctx.r[8].u64 = Load32(memory, ctx.r[11].u32 + ctx.r[27].u32);
	// lwz r9,0(r8)
	ctx.r[9].u64 = Load32(memory, ctx.r[8].u32 + 0);
	// clrlwi r11,r9,27
	ctx.r[11].u64 = ctx.r[9].u32 & 0x1F;
	// rlwinm. r10,r11,0,27,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x18;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// rlwinm. r10,r11,0,30,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aa78
	if (ctx.cr0.eq) goto loc_8305AA78;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aa78
	if (ctx.cr0.eq) goto loc_8305AA78;
	// li r10,0
	ctx.r[10].s64 = 0;
	// b 0x8305aa90
	goto loc_8305AA90;
loc_8305AA78:
	// rlwinm. r10,r11,0,29,29
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aa8c
	if (ctx.cr0.eq) goto loc_8305AA8C;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// li r10,0
	ctx.r[10].s64 = 0;
	// bne 0x8305aa90
	if (!ctx.cr0.eq) goto loc_8305AA90;
loc_8305AA8C:
	// li r10,1
	ctx.r[10].s64 = 1;
loc_8305AA90:
	// clrlwi. r10,r10,24
	ctx.r[10].u64 = ctx.r[10].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x8305aac0
	if (ctx.cr6.eq) goto loc_8305AAC0;
	// lwz r10,12(r8)
	ctx.r[10].u64 = Load32(memory, ctx.r[8].u32 + 12);
	// lwz r7,24(r23)
	ctx.r[7].u64 = Load32(memory, ctx.r[23].u32 + 24);
	// lwz r10,24(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 24);
	// lwz r7,76(r7)
	ctx.r[7].u64 = Load32(memory, ctx.r[7].u32 + 76);
	// lwz r10,76(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 76);
	// xor r10,r10,r7
	ctx.r[10].u64 = ctx.r[10].u64 ^ ctx.r[7].u64;
	// clrlwi. r10,r10,13
	ctx.r[10].u64 = ctx.r[10].u32 & 0x7FFFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
loc_8305AAC0:
	// rlwinm. r7,r11,0,29,29
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[7].s32, 0, ctx.xer);
	// rlwinm r26,r9,27,30,31
	ctx.r[26].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 27) & 0x3;
	// mr r10,r28
	ctx.r[10].u64 = ctx.r[28].u64;
	// beq 0x8305aadc
	if (ctx.cr0.eq) goto loc_8305AADC;
	// rlwinm. r9,r28,0,30,30
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305aadc
	if (ctx.cr0.eq) goto loc_8305AADC;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
loc_8305AADC:
	// and r9,r11,r28
	ctx.r[9].u64 = ctx.r[11].u64 & ctx.r[28].u64;
	// rlwinm. r9,r9,0,29,29
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305aaf0
	if (ctx.cr0.eq) goto loc_8305AAF0;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
	// addi r10,r28,-4
	ctx.r[10].s64 = ctx.r[28].s64 + -4;
loc_8305AAF0:
	// clrlwi. r9,r11,31
	ctx.r[9].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305ab04
	if (ctx.cr0.eq) goto loc_8305AB04;
	// rlwinm. r9,r10,0,30,30
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305ab04
	if (ctx.cr0.eq) goto loc_8305AB04;
	// addi r10,r10,-2
	ctx.r[10].s64 = ctx.r[10].s64 + -2;
loc_8305AB04:
	// lwz r27,12(r8)
	ctx.r[27].u64 = Load32(memory, ctx.r[8].u32 + 12);
	// b 0x8305adc0
	goto loc_8305ADC0;
loc_8305AB0C:
	// rlwinm r11,r22,0,0,19
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[22].u32 | (ctx.r[22].u64 << 32), 0) & 0xFFFFF000;
	// li r6,0
	ctx.r[6].s64 = 0;
	// rlwinm r5,r10,13,29,31
	ctx.r[5].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 13) & 0x7;
	// li r4,102
	ctx.r[4].s64 = 102;
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lwz r3,148(r11)
	ctx.r[3].u64 = Load32(memory, ctx.r[11].u32 + 148);
	// bl 0x82fac238
	ctx.lr = 0x8305AB28;
	CallSelector(ctx, memory);
	// addi r11,r27,-4
	ctx.r[11].s64 = ctx.r[27].s64 + -4;
	// lwzx r11,r3,r11
	ctx.r[11].u64 = Load32(memory, ctx.r[3].u32 + ctx.r[11].u32);
	// cmplwi cr6,r11,1
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 1, ctx.xer);
	// blt cr6,0x8305ab80
	if (ctx.cr6.lt) goto loc_8305AB80;
	// beq cr6,0x8305ab70
	if (ctx.cr6.eq) goto loc_8305AB70;
	// cmplwi cr6,r11,3
	ctx.cr6.compare<uint32_t>(ctx.r[11].u32, 3, ctx.xer);
	// blt cr6,0x8305ab5c
	if (ctx.cr6.lt) goto loc_8305AB5C;
	// bne cr6,0x8305aebc
	if (!ctx.cr6.eq) goto loc_8305AEBC;
	// li r25,4
	ctx.r[25].s64 = 4;
	// cmplwi cr6,r24,0
	ctx.cr6.compare<uint32_t>(ctx.r[24].u32, 0, ctx.xer);
	// bne cr6,0x8305ab88
	if (!ctx.cr6.eq) goto loc_8305AB88;
	// li r24,4
	ctx.r[24].s64 = 4;
	// b 0x8305ab88
	goto loc_8305AB88;
loc_8305AB5C:
	// li r25,3
	ctx.r[25].s64 = 3;
	// cmplwi cr6,r24,0
	ctx.cr6.compare<uint32_t>(ctx.r[24].u32, 0, ctx.xer);
	// bne cr6,0x8305ab78
	if (!ctx.cr6.eq) goto loc_8305AB78;
	// li r24,3
	ctx.r[24].s64 = 3;
	// b 0x8305ab78
	goto loc_8305AB78;
loc_8305AB70:
	// li r25,2
	ctx.r[25].s64 = 2;
	// li r24,2
	ctx.r[24].s64 = 2;
loc_8305AB78:
	// rlwinm r28,r28,0,31,29
	ctx.r[28].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 0) & 0xFFFFFFFFFFFFFFFD;
	// b 0x8305ab88
	goto loc_8305AB88;
loc_8305AB80:
	// li r25,1
	ctx.r[25].s64 = 1;
	// li r24,1
	ctx.r[24].s64 = 1;
loc_8305AB88:
	// lwz r9,40(r27)
	ctx.r[9].u64 = Load32(memory, ctx.r[27].u32 + 40);
	// lwz r8,0(r9)
	ctx.r[8].u64 = Load32(memory, ctx.r[9].u32 + 0);
	// clrlwi r11,r8,27
	ctx.r[11].u64 = ctx.r[8].u32 & 0x1F;
	// rlwinm. r10,r11,0,27,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x18;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// rlwinm. r10,r11,0,30,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305abb4
	if (ctx.cr0.eq) goto loc_8305ABB4;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305abb4
	if (ctx.cr0.eq) goto loc_8305ABB4;
	// li r10,0
	ctx.r[10].s64 = 0;
	// b 0x8305abcc
	goto loc_8305ABCC;
loc_8305ABB4:
	// rlwinm. r10,r11,0,29,29
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305abc8
	if (ctx.cr0.eq) goto loc_8305ABC8;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// li r10,0
	ctx.r[10].s64 = 0;
	// bne 0x8305abcc
	if (!ctx.cr0.eq) goto loc_8305ABCC;
loc_8305ABC8:
	// li r10,1
	ctx.r[10].s64 = 1;
loc_8305ABCC:
	// clrlwi. r10,r10,24
	ctx.r[10].u64 = ctx.r[10].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x8305abfc
	if (ctx.cr6.eq) goto loc_8305ABFC;
	// lwz r10,12(r9)
	ctx.r[10].u64 = Load32(memory, ctx.r[9].u32 + 12);
	// lwz r7,24(r23)
	ctx.r[7].u64 = Load32(memory, ctx.r[23].u32 + 24);
	// lwz r10,24(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 24);
	// lwz r7,76(r7)
	ctx.r[7].u64 = Load32(memory, ctx.r[7].u32 + 76);
	// lwz r10,76(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 76);
	// xor r10,r10,r7
	ctx.r[10].u64 = ctx.r[10].u64 ^ ctx.r[7].u64;
	// clrlwi. r10,r10,13
	ctx.r[10].u64 = ctx.r[10].u32 & 0x7FFFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
loc_8305ABFC:
	// lwz r27,12(r9)
	ctx.r[27].u64 = Load32(memory, ctx.r[9].u32 + 12);
	// rlwinm r9,r8,27,24,31
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 27) & 0xFF;
	// rlwinm r7,r26,1,0,30
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 1) & 0xFFFFFFFE;
	// rlwinm. r8,r11,0,29,29
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[8].s32, 0, ctx.xer);
	// srw r9,r9,r7
	ctx.r[9].u64 = ctx.r[7].u8 & 0x20 ? 0 : (ctx.r[9].u32 >> (ctx.r[7].u8 & 0x3F));
	// b 0x8305ad80
	goto loc_8305AD80;
loc_8305AC14:
	// lwz r11,40(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 40);
	// lwz r30,44(r27)
	ctx.r[30].u64 = Load32(memory, ctx.r[27].u32 + 44);
	// lwz r29,48(r27)
	ctx.r[29].u64 = Load32(memory, ctx.r[27].u32 + 48);
	// lwz r3,12(r11)
	ctx.r[3].u64 = Load32(memory, ctx.r[11].u32 + 12);
	// lwz r10,8(r3)
	ctx.r[10].u64 = Load32(memory, ctx.r[3].u32 + 8);
	// rlwinm r10,r10,0,18,24
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x3F80;
	// cmplwi cr6,r10,15872
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 15872, ctx.xer);
	// bne cr6,0x8305aebc
	if (!ctx.cr6.eq) goto loc_8305AEBC;
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// rlwinm r31,r26,1,0,30
	ctx.r[31].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 1) & 0xFFFFFFFE;
	// rlwinm r10,r11,27,24,31
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 27) & 0xFF;
	// clrlwi r5,r11,27
	ctx.r[5].u64 = ctx.r[11].u32 & 0x1F;
	// srw r11,r10,r31
	ctx.r[11].u64 = ctx.r[31].u8 & 0x20 ? 0 : (ctx.r[10].u32 >> (ctx.r[31].u8 & 0x3F));
	// clrlwi r4,r11,30
	ctx.r[4].u64 = ctx.r[11].u32 & 0x3;
	// bl 0x83053ea0
	ctx.lr = 0x8305AC50;
	CallNumeric(0x83053ea0u, ctx, memory, dependencies);
	// fcmpu cr6,f1,f31
	DisableFlush(ctx, dependencies.numeric);
	CompareFp(ctx.cr6, ctx.f1.u64, ctx.f31.u64);
	// mr r9,r30
	ctx.r[9].u64 = ctx.r[30].u64;
	// beq cr6,0x8305ac60
	if (ctx.cr6.eq) goto loc_8305AC60;
	// mr r9,r29
	ctx.r[9].u64 = ctx.r[29].u64;
loc_8305AC60:
	// lwz r8,0(r9)
	ctx.r[8].u64 = Load32(memory, ctx.r[9].u32 + 0);
	// clrlwi r11,r8,27
	ctx.r[11].u64 = ctx.r[8].u32 & 0x1F;
	// rlwinm. r10,r11,0,27,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x18;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// rlwinm. r10,r11,0,30,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305ac88
	if (ctx.cr0.eq) goto loc_8305AC88;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305ac88
	if (ctx.cr0.eq) goto loc_8305AC88;
	// li r10,0
	ctx.r[10].s64 = 0;
	// b 0x8305aca0
	goto loc_8305ACA0;
loc_8305AC88:
	// rlwinm. r10,r11,0,29,29
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305ac9c
	if (ctx.cr0.eq) goto loc_8305AC9C;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// li r10,0
	ctx.r[10].s64 = 0;
	// bne 0x8305aca0
	if (!ctx.cr0.eq) goto loc_8305ACA0;
loc_8305AC9C:
	// li r10,1
	ctx.r[10].s64 = 1;
loc_8305ACA0:
	// clrlwi. r10,r10,24
	ctx.r[10].u64 = ctx.r[10].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x8305acd0
	if (ctx.cr6.eq) goto loc_8305ACD0;
	// lwz r10,12(r9)
	ctx.r[10].u64 = Load32(memory, ctx.r[9].u32 + 12);
	// lwz r7,24(r23)
	ctx.r[7].u64 = Load32(memory, ctx.r[23].u32 + 24);
	// lwz r10,24(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 24);
	// lwz r7,76(r7)
	ctx.r[7].u64 = Load32(memory, ctx.r[7].u32 + 76);
	// lwz r10,76(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 76);
	// xor r10,r10,r7
	ctx.r[10].u64 = ctx.r[10].u64 ^ ctx.r[7].u64;
	// clrlwi. r10,r10,13
	ctx.r[10].u64 = ctx.r[10].u32 & 0x7FFFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
loc_8305ACD0:
	// rlwinm r8,r8,27,24,31
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[8].u32 | (ctx.r[8].u64 << 32), 27) & 0xFF;
	// lwz r27,12(r9)
	ctx.r[27].u64 = Load32(memory, ctx.r[9].u32 + 12);
	// rlwinm. r9,r11,0,29,29
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// srw r9,r8,r31
	ctx.r[9].u64 = ctx.r[31].u8 & 0x20 ? 0 : (ctx.r[8].u32 >> (ctx.r[31].u8 & 0x3F));
	// b 0x8305ad80
	goto loc_8305AD80;
loc_8305ACE4:
	// lwz r31,40(r27)
	ctx.r[31].u64 = Load32(memory, ctx.r[27].u32 + 40);
	// lwz r4,44(r27)
	ctx.r[4].u64 = Load32(memory, ctx.r[27].u32 + 44);
	// mr r3,r31
	ctx.r[3].u64 = ctx.r[31].u64;
	// bl 0x82fac128
	ctx.lr = 0x8305ACF4;
	CallNumeric(0x82fac128u, ctx, memory, dependencies);
	// clrlwi. r11,r3,24
	ctx.r[11].u64 = ctx.r[3].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// lwz r9,0(r31)
	ctx.r[9].u64 = Load32(memory, ctx.r[31].u32 + 0);
	// clrlwi r11,r9,27
	ctx.r[11].u64 = ctx.r[9].u32 & 0x1F;
	// rlwinm. r10,r11,0,27,28
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x18;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
	// rlwinm. r10,r11,0,30,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305ad24
	if (ctx.cr0.eq) goto loc_8305AD24;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305ad24
	if (ctx.cr0.eq) goto loc_8305AD24;
	// li r10,0
	ctx.r[10].s64 = 0;
	// b 0x8305ad3c
	goto loc_8305AD3C;
loc_8305AD24:
	// rlwinm. r10,r11,0,29,29
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305ad38
	if (ctx.cr0.eq) goto loc_8305AD38;
	// clrlwi. r10,r28,31
	ctx.r[10].u64 = ctx.r[28].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// li r10,0
	ctx.r[10].s64 = 0;
	// bne 0x8305ad3c
	if (!ctx.cr0.eq) goto loc_8305AD3C;
loc_8305AD38:
	// li r10,1
	ctx.r[10].s64 = 1;
loc_8305AD3C:
	// clrlwi. r10,r10,24
	ctx.r[10].u64 = ctx.r[10].u32 & 0xFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// cmplwi cr6,r23,0
	ctx.cr6.compare<uint32_t>(ctx.r[23].u32, 0, ctx.xer);
	// beq cr6,0x8305ad6c
	if (ctx.cr6.eq) goto loc_8305AD6C;
	// lwz r10,12(r31)
	ctx.r[10].u64 = Load32(memory, ctx.r[31].u32 + 12);
	// lwz r8,24(r23)
	ctx.r[8].u64 = Load32(memory, ctx.r[23].u32 + 24);
	// lwz r10,24(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 24);
	// lwz r8,76(r8)
	ctx.r[8].u64 = Load32(memory, ctx.r[8].u32 + 76);
	// lwz r10,76(r10)
	ctx.r[10].u64 = Load32(memory, ctx.r[10].u32 + 76);
	// xor r10,r10,r8
	ctx.r[10].u64 = ctx.r[10].u64 ^ ctx.r[8].u64;
	// clrlwi. r10,r10,13
	ctx.r[10].u64 = ctx.r[10].u32 & 0x7FFFF;
	ctx.cr0.compare<int32_t>(ctx.r[10].s32, 0, ctx.xer);
	// bne 0x8305aebc
	if (!ctx.cr0.eq) goto loc_8305AEBC;
loc_8305AD6C:
	// rlwinm r8,r26,1,0,30
	ctx.r[8].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 1) & 0xFFFFFFFE;
	// lwz r27,12(r31)
	ctx.r[27].u64 = Load32(memory, ctx.r[31].u32 + 12);
	// rlwinm r9,r9,27,24,31
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 27) & 0xFF;
	// rlwinm. r7,r11,0,29,29
	ctx.r[7].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[7].s32, 0, ctx.xer);
	// srw r9,r9,r8
	ctx.r[9].u64 = ctx.r[8].u8 & 0x20 ? 0 : (ctx.r[9].u32 >> (ctx.r[8].u8 & 0x3F));
loc_8305AD80:
	// mr r10,r28
	ctx.r[10].u64 = ctx.r[28].u64;
	// clrlwi r26,r9,30
	ctx.r[26].u64 = ctx.r[9].u32 & 0x3;
	// beq 0x8305ad98
	if (ctx.cr0.eq) goto loc_8305AD98;
	// rlwinm. r9,r28,0,30,30
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[28].u32 | (ctx.r[28].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305ad98
	if (ctx.cr0.eq) goto loc_8305AD98;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
loc_8305AD98:
	// and r9,r11,r28
	ctx.r[9].u64 = ctx.r[11].u64 & ctx.r[28].u64;
	// rlwinm. r9,r9,0,29,29
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[9].u32 | (ctx.r[9].u64 << 32), 0) & 0x4;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305adac
	if (ctx.cr0.eq) goto loc_8305ADAC;
	// addi r11,r11,-4
	ctx.r[11].s64 = ctx.r[11].s64 + -4;
	// addi r10,r28,-4
	ctx.r[10].s64 = ctx.r[28].s64 + -4;
loc_8305ADAC:
	// clrlwi. r9,r11,31
	ctx.r[9].u64 = ctx.r[11].u32 & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305adc0
	if (ctx.cr0.eq) goto loc_8305ADC0;
	// rlwinm. r9,r10,0,30,30
	ctx.r[9].u64 = __builtin_rotateleft64(ctx.r[10].u32 | (ctx.r[10].u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r[9].s32, 0, ctx.xer);
	// beq 0x8305adc0
	if (ctx.cr0.eq) goto loc_8305ADC0;
	// addi r10,r10,-2
	ctx.r[10].s64 = ctx.r[10].s64 + -2;
loc_8305ADC0:
	// or r28,r11,r10
	ctx.r[28].u64 = ctx.r[11].u64 | ctx.r[10].u64;
loc_8305ADC4:
	// and. r11,r28,r21
	ctx.r[11].u64 = ctx.r[28].u64 & ctx.r[21].u64;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8305a8c8
	if (ctx.cr0.eq) goto loc_8305A8C8;
	// b 0x8305aebc
	goto loc_8305AEBC;
loc_8305ADD0:
	// cmpwi cr6,r11,124
	ctx.cr6.compare<int32_t>(ctx.r[11].s32, 124, ctx.xer);
	// bne cr6,0x8305aebc
	if (!ctx.cr6.eq) goto loc_8305AEBC;
	// cmplwi cr6,r25,0
	ctx.cr6.compare<uint32_t>(ctx.r[25].u32, 0, ctx.xer);
	// beq cr6,0x8305aebc
	if (ctx.cr6.eq) goto loc_8305AEBC;
	// rlwinm r11,r27,0,0,19
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[27].u32 | (ctx.r[27].u64 << 32), 0) & 0xFFFFF000;
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lwz r11,148(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 148);
	// lwz r11,40(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 40);
	// not r11,r11
	ctx.r[11].u64 = ~ctx.r[11].u64;
	// rlwinm. r11,r11,18,31,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 18) & 0x1;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// lwz r11,16(r27)
	ctx.r[11].u64 = Load32(memory, ctx.r[27].u32 + 16);
	// rlwinm r10,r26,1,0,30
	ctx.r[10].u64 = __builtin_rotateleft64(ctx.r[26].u32 | (ctx.r[26].u64 << 32), 1) & 0xFFFFFFFE;
	// rlwinm r11,r11,18,24,31
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 18) & 0xFF;
	// srw r11,r11,r10
	ctx.r[11].u64 = ctx.r[10].u8 & 0x20 ? 0 : (ctx.r[11].u32 >> (ctx.r[10].u8 & 0x3F));
	// clrlwi. r11,r11,30
	ctx.r[11].u64 = ctx.r[11].u32 & 0x3;
	ctx.cr0.compare<int32_t>(ctx.r[11].s32, 0, ctx.xer);
	// beq 0x8305aebc
	if (ctx.cr0.eq) goto loc_8305AEBC;
	// addi r11,r26,10
	ctx.r[11].s64 = ctx.r[26].s64 + 10;
	// addi r10,r25,-1
	ctx.r[10].s64 = ctx.r[25].s64 + -1;
	// rlwinm r11,r11,2,0,29
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[11].u32 | (ctx.r[11].u64 << 32), 2) & 0xFFFFFFFC;
	// cmplwi cr6,r10,1
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 1, ctx.xer);
	// lwzx r11,r11,r27
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + ctx.r[27].u32);
	// blt cr6,0x8305ae8c
	if (ctx.cr6.lt) goto loc_8305AE8C;
	// beq cr6,0x8305ae7c
	if (ctx.cr6.eq) goto loc_8305AE7C;
	// cmplwi cr6,r10,3
	ctx.cr6.compare<uint32_t>(ctx.r[10].u32, 3, ctx.xer);
	// blt cr6,0x8305ae64
	if (ctx.cr6.lt) goto loc_8305AE64;
	// beq cr6,0x8305ae50
	if (ctx.cr6.eq) goto loc_8305AE50;
	// rlwinm r11,r22,0,0,19
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[22].u32 | (ctx.r[22].u64 << 32), 0) & 0xFFFFF000;
	// li r4,4800
	ctx.r[4].s64 = 4800;
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lwz r3,148(r11)
	ctx.r[3].u64 = Load32(memory, ctx.r[11].u32 + 148);
	// bl 0x82f99f48
	ctx.lr = 0x8305AE50;
	CallDiagnostic(ctx, memory, dependencies);
loc_8305AE50:
	// extsw r11,r11
	ctx.r[11].s64 = ctx.r[11].s32;
	// li r5,1
	ctx.r[5].s64 = 1;
	// std r11,80(r1)
	Store64(memory, ctx.r[1].u32 + 80, ctx.r[11].u64);
	// lfd f0,80(r1)
	DisableFlush(ctx, dependencies.numeric);
	ctx.f0.u64 = Load64(memory, ctx.r[1].u32 + 80);
	// b 0x8305ae74
	goto loc_8305AE74;
loc_8305AE64:
	// clrldi r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 & 0xFFFFFFFF;
	// li r5,2
	ctx.r[5].s64 = 2;
	// std r11,80(r1)
	Store64(memory, ctx.r[1].u32 + 80, ctx.r[11].u64);
	// lfd f0,80(r1)
	DisableFlush(ctx, dependencies.numeric);
	ctx.f0.u64 = Load64(memory, ctx.r[1].u32 + 80);
loc_8305AE74:
	// fcfid f1,f0
	DisableFlush(ctx, dependencies.numeric);
	ctx.f1.f64 = double(ctx.f0.s64);
	// b 0x8305aea4
	goto loc_8305AEA4;
loc_8305AE7C:
	// clrldi r11,r11,32
	ctx.r[11].u64 = ctx.r[11].u64 & 0xFFFFFFFF;
	// std r11,80(r1)
	Store64(memory, ctx.r[1].u32 + 80, ctx.r[11].u64);
	// lfd f0,80(r1)
	DisableFlush(ctx, dependencies.numeric);
	ctx.f0.u64 = Load64(memory, ctx.r[1].u32 + 80);
	// b 0x8305ae98
	goto loc_8305AE98;
loc_8305AE8C:
	// extsw r11,r11
	ctx.r[11].s64 = ctx.r[11].s32;
	// std r11,80(r1)
	Store64(memory, ctx.r[1].u32 + 80, ctx.r[11].u64);
	// lfd f0,80(r1)
	DisableFlush(ctx, dependencies.numeric);
	ctx.f0.u64 = Load64(memory, ctx.r[1].u32 + 80);
loc_8305AE98:
	// fcfid f0,f0
	DisableFlush(ctx, dependencies.numeric);
	ctx.f0.f64 = double(ctx.f0.s64);
	// li r5,0
	ctx.r[5].s64 = 0;
	// frsp f1,f0
	ctx.f1.f64 = double(float(ctx.f0.f64));
loc_8305AEA4:
	// rlwinm r11,r22,0,0,19
	ctx.r[11].u64 = __builtin_rotateleft64(ctx.r[22].u32 | (ctx.r[22].u64 << 32), 0) & 0xFFFFF000;
	// lwz r11,0(r11)
	ctx.r[11].u64 = Load32(memory, ctx.r[11].u32 + 0);
	// lwz r3,148(r11)
	ctx.r[3].u64 = Load32(memory, ctx.r[11].u32 + 148);
	// bl 0x8305a7c0
	ctx.lr = 0x8305AEB4;
	CallScalar(ctx, memory, dependencies);
	// mr r27,r3
	ctx.r[27].u64 = ctx.r[3].u64;
	// li r26,0
	ctx.r[26].s64 = 0;
loc_8305AEBC:
	// stw r27,0(r19)
	Store32(memory, ctx.r[19].u32 + 0, ctx.r[27].u32);
	// stw r26,0(r18)
	Store32(memory, ctx.r[18].u32 + 0, ctx.r[26].u32);
	// stw r28,0(r17)
	Store32(memory, ctx.r[17].u32 + 0, ctx.r[28].u32);
	// addi r1,r1,224
	ctx.r[1].s64 = ctx.r[1].s64 + 224;
	// lfd f31,-136(r1)
	DisableFlush(ctx, dependencies.numeric);
	ctx.f31.u64 = Load64(memory, ctx.r[1].u32 + -136);
	// b 0x82b7a70c
	Restore17(ctx, memory);
	return;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    if (entry != 0x8305a868u) return false;
    auto context = ToContext(registers);
    Body(context, memory, dependencies);
    FromContext(context, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_descriptor_match_caller
