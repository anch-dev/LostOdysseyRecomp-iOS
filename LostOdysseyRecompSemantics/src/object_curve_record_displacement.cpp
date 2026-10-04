#include "lo_semantics/object_curve_record_displacement.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_record_displacement
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& s, unsigned index) { return s.r[index]; }
std::uint64_t& FP(Registers& s, unsigned index)
{
    std::uint64_t* fields[] = {&s.f0_bits, &s.f1_bits, &s.f2_bits,
        &s.f3_bits, &s.f4_bits, &s.f5_bits, &s.f6_bits, &s.f7_bits,
        &s.f8_bits, &s.f9_bits, &s.f10_bits, &s.f11_bits, &s.f12_bits,
        &s.f13_bits, &s.f14_bits, &s.f15_bits, &s.f16_bits, &s.f17_bits,
        &s.f18_bits, &s.f19_bits, &s.f20_bits, &s.f21_bits, &s.f22_bits,
        &s.f23_bits, &s.f24_bits, &s.f25_bits, &s.f26_bits, &s.f27_bits,
        &s.f28_bits, &s.f29_bits, &s.f30_bits, &s.f31_bits};
    return *fields[index];
}
std::uint32_t W(std::uint64_t value) { return Address(value); }
std::int32_t S(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(W(value)); }
double F(std::uint64_t value) { return std::bit_cast<double>(value); }
std::uint64_t Bits(double value) { return std::bit_cast<std::uint64_t>(value); }
std::uint64_t Single(double value) { return Bits(double(float(value))); }
void CmpS(Registers& s, std::uint64_t a, std::uint64_t b)
{
    s.cr6 = {std::uint8_t(S(a) < S(b)), std::uint8_t(S(a) > S(b)),
        std::uint8_t(S(a) == S(b)), s.xer_so};
}
void CmpU(Registers& s, std::uint64_t a, std::uint64_t b)
{
    s.cr6 = {std::uint8_t(W(a) < W(b)), std::uint8_t(W(a) > W(b)),
        std::uint8_t(W(a) == W(b)), s.xer_so};
}
void CmpF(Registers& s, unsigned a, unsigned b)
{
    const double x = F(FP(s, a)), y = F(FP(s, b));
    const bool un = std::isnan(x) || std::isnan(y);
    s.cr6 = {std::uint8_t(!un && x < y), std::uint8_t(!un && x > y),
        std::uint8_t(!un && x == y), std::uint8_t(un)};
}
void DisableFlush(NativeServices& native, Registers& s)
{
    constexpr std::uint32_t Mask = 0x8040u;
    if (s.cached_fp_control & Mask)
    {
        s.cached_fp_control &= ~Mask;
        native.SetHostFpControl(s.cached_fp_control);
    }
}
void Load(GuestMemory& m, NativeServices& native, Registers& s,
    unsigned dest, std::uint64_t address)
{
    DisableFlush(native, s);
    FP(s, dest) = LoadedSingle::FromWord(m.ReadU32(W(address))).FprBits();
}
void Store(GuestMemory& m, Registers& s, unsigned source,
    std::uint64_t address)
{ m.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(FP(s, source))))); }
void Mov(Registers& s, unsigned d, unsigned a) { FP(s, d) = FP(s, a); }
void Mul(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) * F(FP(s, b))); }
void Add(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) + F(FP(s, b))); }
void Sub(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) - F(FP(s, b))); }
void Madd(Registers& s, unsigned d, unsigned a, unsigned b, unsigned c)
{ FP(s, d) = Single(std::fma(F(FP(s, a)), F(FP(s, b)), F(FP(s, c)))); }
void Msub(Registers& s, unsigned d, unsigned a, unsigned b, unsigned c)
{ FP(s, d) = Single(std::fma(F(FP(s, a)), F(FP(s, b)), -F(FP(s, c)))); }
void Divs(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) / F(FP(s, b))); }

class LowerServices final : public object_curve_sample::NativeServices,
    public object_sample_accumulator::NativeServices
{
public:
    explicit LowerServices(object_curve_record_displacement::NativeServices& native)
        : native_(native) {}
    void SetHostFpControl(std::uint32_t control) override
    { native_.SetHostFpControl(control); }
    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_curve_sample::Registers& state) override
    { native_.CallVirtual(target, memory, static_cast<Registers&>(state)); }
private:
    object_curve_record_displacement::NativeServices& native_;
};

void Normalize(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 11) = R(s, 3);
    R(s, 10) = 0xffffffff82190000ull;
    Load(m, native, s, 12, R(s, 11) + 4u);
    Mul(s, 0, 12, 12);
    Load(m, native, s, 13, R(s, 11));
    Load(m, native, s, 11, R(s, 11) + 8u);
    Load(m, native, s, 10, R(s, 10) - 27252u);
    Madd(s, 0, 13, 13, 0);
    Madd(s, 0, 11, 11, 0);
    CmpF(s, 0, 10);
    if (s.cr6.eq) { R(s, 3) = 1; return; }
    CmpF(s, 0, 1);
    if (s.cr6.lt) { R(s, 3) = 0; return; }
    FP(s, 10) = Bits(std::sqrt(F(FP(s, 0))));
    R(s, 10) = 0xffffffff82000000ull;
    R(s, 3) = 1;
    FP(s, 0) = ReadU64(m, W(R(s, 10) + 3880u));
    FP(s, 0) = Bits(F(FP(s, 0)) / F(FP(s, 10)));
    FP(s, 0) = Single(F(FP(s, 0)));
    Mul(s, 13, 13, 0); Store(m, s, 13, R(s, 11));
    Mul(s, 13, 12, 0); Store(m, s, 13, R(s, 11) + 4u);
    Mul(s, 0, 11, 0); Store(m, s, 0, R(s, 11) + 8u);
}

void Save(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x826276c0u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 24u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    R(s, 12) = R(s, 1) - 72u;
    s.lr = 0x826276c8u;
    DisableFlush(native, s);
    for (unsigned i = 14u; i <= 31u; ++i)
        WriteU64(m, old_sp - 80u - (31u - i) * 8u, FP(s, i));
    R(s, 1) -= 368u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 1) += 368u;
    R(s, 12) = R(s, 1) - 72u;
    s.lr = 0x82627c90u;
    DisableFlush(native, s);
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 14u; i <= 31u; ++i)
        FP(s, i) = ReadU64(m, old_sp - 80u - (31u - i) * 8u);
    for (unsigned i = 24u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, old_sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(old_sp - 8u);
    s.lr = R(s, 12);
}

void Transform(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    const auto object = R(s, 30), params = R(s, 29);
    R(s, 10) = m.ReadU32(W(object + 72u));
    R(s, 11) = m.ReadU32(W(params + 16u));
    m.WriteU32(sp + 88u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 76u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 72u));
    m.WriteU32(sp + 92u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 80u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 68u));
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    m.WriteU32(sp + 96u, W(R(s, 10)));
    CmpU(s, R(s, 11), 0u);
    R(s, 10) = m.ReadU32(W(object + 84u));
    R(s, 11) = m.ReadU32(W(object + 68u));
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    m.WriteU32(sp + 104u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 88u));
    m.WriteU32(sp + 108u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 92u));
    m.WriteU32(sp + 112u, W(R(s, 10)));
    if (s.cr6.eq)
    {
        CmpU(s, R(s, 11), 0u);
        if (s.cr6.eq) return;
        R(s, 11) = m.ReadU32(W(params + 8u));
        const auto matrix = R(s, 11);
        Load(m, native, s, 0, object + 76u);
        Load(m, native, s, 13, object + 80u);
        Load(m, native, s, 12, object + 88u);
        Load(m, native, s, 10, object + 72u);
        Load(m, native, s, 11, object + 92u);
        Load(m, native, s, 8, matrix + 128u); Mul(s, 8, 8, 0);
        Load(m, native, s, 7, matrix + 132u);
        Load(m, native, s, 6, matrix + 136u);
        Mul(s, 7, 7, 0); Mul(s, 0, 6, 0);
        Load(m, native, s, 6, matrix + 144u);
        Load(m, native, s, 3, matrix + 148u);
        Mov(s, 1, 6);
        Load(m, native, s, 2, matrix + 152u);
        Mov(s, 31, 3);
        Load(m, native, s, 30, matrix + 112u);
        Load(m, native, s, 5, matrix + 128u);
        Load(m, native, s, 4, matrix + 132u);
        Mul(s, 5, 5, 12);
        Load(m, native, s, 26, matrix + 136u);
        Mul(s, 4, 4, 12); Mul(s, 12, 26, 12);
        Load(m, native, s, 29, matrix + 120u);
        Madd(s, 8, 6, 13, 8);
        Load(m, native, s, 6, matrix + 116u);
        Madd(s, 7, 3, 13, 7);
        Load(m, native, s, 9, object + 84u);
        Madd(s, 0, 2, 13, 0);
        Load(m, native, s, 13, matrix + 160u);
        Mov(s, 3, 30);
        Load(m, native, s, 2, matrix + 164u);
        Mov(s, 28, 6); Mov(s, 26, 29); Mov(s, 27, 13);
        Madd(s, 5, 1, 11, 5);
        Load(m, native, s, 1, matrix + 168u);
        Madd(s, 4, 31, 11, 4);
        Mov(s, 31, 2);
        Madd(s, 8, 30, 10, 8);
        Load(m, native, s, 30, matrix + 152u);
        Madd(s, 7, 6, 10, 7);
        Madd(s, 10, 29, 10, 0);
        Mov(s, 6, 1);
        Madd(s, 5, 9, 3, 5);
        Madd(s, 4, 28, 9, 4);
        Add(s, 0, 8, 13); Store(m, s, 0, sp + 120u);
        Madd(s, 8, 30, 11, 12);
        R(s, 11) = m.ReadU32(sp + 120u);
        Add(s, 13, 7, 2); Store(m, s, 13, sp + 124u);
        Add(s, 12, 10, 1); Store(m, s, 12, sp + 128u);
        m.WriteU32(sp + 88u, W(R(s, 11)));
        R(s, 11) = m.ReadU32(sp + 124u);
        Add(s, 11, 5, 27); Add(s, 10, 4, 31);
        Store(m, s, 11, sp + 120u); Store(m, s, 10, sp + 124u);
        Madd(s, 9, 26, 9, 8);
        m.WriteU32(sp + 92u, W(R(s, 11)));
        R(s, 11) = m.ReadU32(sp + 128u);
        Add(s, 9, 9, 6); Store(m, s, 9, sp + 128u);
    }
    else
    {
        CmpU(s, R(s, 11), 0u);
        if (!s.cr6.eq) return;
        R(s, 11) = m.ReadU32(W(params + 8u));
        const auto matrix = R(s, 11);
        Load(m, native, s, 13, object + 76u);
        Load(m, native, s, 0, object + 72u);
        Load(m, native, s, 12, object + 80u);
        Load(m, native, s, 10, object + 92u);
        Load(m, native, s, 9, object + 84u);
        Load(m, native, s, 7, matrix + 164u);
        Load(m, native, s, 8, matrix + 160u);
        Sub(s, 13, 13, 7); Sub(s, 0, 0, 8);
        Load(m, native, s, 8, matrix + 168u);
        Sub(s, 10, 10, 8);
        Load(m, native, s, 11, object + 88u);
        Sub(s, 12, 12, 8);
        Load(m, native, s, 8, matrix + 160u);
        Sub(s, 9, 9, 8);
        Load(m, native, s, 8, matrix + 116u); Mov(s, 5, 8);
        Load(m, native, s, 6, matrix + 144u);
        Sub(s, 11, 11, 7);
        Load(m, native, s, 7, matrix + 128u); Mov(s, 23, 7);
        Load(m, native, s, 2, matrix + 120u); Mov(s, 22, 6);
        Load(m, native, s, 4, matrix + 132u);
        Load(m, native, s, 1, matrix + 136u); Mov(s, 30, 2);
        Mul(s, 8, 8, 13);
        Load(m, native, s, 3, matrix + 148u);
        Mul(s, 7, 7, 0);
        Load(m, native, s, 31, matrix + 152u);
        Mul(s, 6, 6, 0);
        Load(m, native, s, 27, matrix + 112u);
        Mov(s, 26, 4); Mov(s, 29, 1); Mov(s, 28, 31);
        Mul(s, 5, 5, 11); Mul(s, 4, 4, 11); Mul(s, 11, 3, 11);
        Mov(s, 25, 3);
        Madd(s, 8, 2, 12, 8);
        Madd(s, 7, 1, 12, 7);
        Madd(s, 12, 31, 12, 6);
        Mov(s, 24, 27);
        Madd(s, 6, 30, 10, 5);
        Madd(s, 5, 29, 10, 4);
        Madd(s, 11, 28, 10, 11);
        Madd(s, 0, 27, 0, 8);
        Store(m, s, 0, sp + 120u);
        R(s, 11) = m.ReadU32(sp + 120u);
        Madd(s, 0, 26, 13, 7); Store(m, s, 0, sp + 124u);
        Madd(s, 0, 25, 13, 12); Store(m, s, 0, sp + 128u);
        m.WriteU32(sp + 88u, W(R(s, 11)));
        Madd(s, 0, 24, 9, 6);
        R(s, 11) = m.ReadU32(sp + 124u);
        Store(m, s, 0, sp + 120u);
        Madd(s, 0, 23, 9, 5); Store(m, s, 0, sp + 124u);
        Madd(s, 0, 22, 9, 11);
        m.WriteU32(sp + 92u, W(R(s, 11)));
        R(s, 11) = m.ReadU32(sp + 128u);
        Store(m, s, 0, sp + 128u);
    }
    m.WriteU32(sp + 96u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 120u);
    m.WriteU32(sp + 104u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 124u);
    m.WriteU32(sp + 108u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 128u);
    m.WriteU32(sp + 112u, W(R(s, 11)));
}

void PrepareDirection(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    Load(m, native, s, 0, sp + 104u);
    R(s, 3) = R(s, 1) + 120u;
    Load(m, native, s, 20, sp + 88u);
    Sub(s, 31, 0, 20);
    Store(m, s, 31, sp + 136u);
    R(s, 11) = m.ReadU32(sp + 136u);
    Load(m, native, s, 0, sp + 108u);
    Load(m, native, s, 19, sp + 92u);
    Sub(s, 29, 0, 19);
    Store(m, s, 29, sp + 140u);
    Load(m, native, s, 0, sp + 112u);
    Load(m, native, s, 18, sp + 96u);
    m.WriteU32(sp + 120u, W(R(s, 11)));
    Sub(s, 28, 0, 18);
    R(s, 11) = m.ReadU32(sp + 140u);
    Store(m, s, 28, sp + 144u);
    m.WriteU32(sp + 124u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 144u);
    m.WriteU32(sp + 128u, W(R(s, 11)));
    R(s, 11) = 0xffffffff82000000ull;
    Load(m, native, s, 1, R(s, 11) + 14596u);
    Store(m, s, 1, sp + 84u);
    s.lr = 0x826279d4u;
    Normalize(m, native, s);
}

void ApplyOne(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s)
{
    const auto sp = W(R(s, 1));
    R(s, 11) = m.ReadU16(W(R(s, 28)));
    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
        std::int64_t(S(R(s, 24))));
    R(s, 31) = R(s, 11) + R(s, 25);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 92u));
    R(s, 11) = W(R(s, 11)) & 1u;
    CmpS(s, R(s, 11), 0u);
    if (!s.cr6.eq) return;
    R(s, 11) = R(s, 31) + 16u;
    Mul(s, 0, 29, 29);
    Mov(s, 9, 30); Mov(s, 8, 30); Mov(s, 7, 30);
    CmpF(s, 31, 30);
    R(s, 10) = m.ReadU32(W(R(s, 11) + 4u));
    m.WriteU32(sp + 140u, W(R(s, 10)));
    Load(m, native, s, 23, sp + 140u);
    Sub(s, 13, 23, 19);
    R(s, 10) = m.ReadU32(W(R(s, 11) + 8u));
    R(s, 11) = m.ReadU32(W(R(s, 11)));
    Madd(s, 0, 28, 28, 0);
    m.WriteU32(sp + 144u, W(R(s, 10)));
    Load(m, native, s, 22, sp + 144u);
    Sub(s, 12, 22, 18);
    m.WriteU32(sp + 136u, W(R(s, 11)));
    Load(m, native, s, 21, sp + 136u);
    Sub(s, 11, 21, 20);
    Mul(s, 13, 13, 29);
    Madd(s, 0, 31, 31, 0);
    Madd(s, 13, 12, 28, 13);
    Divs(s, 0, 17, 0);
    Madd(s, 13, 11, 31, 13);
    Mul(s, 12, 13, 29); Mul(s, 11, 13, 28);
    Mul(s, 10, 13, 31);
    Mul(s, 13, 12, 0); Mul(s, 12, 11, 0);
    Mul(s, 0, 0, 10);
    Mul(s, 13, 13, 13);
    Madd(s, 13, 12, 12, 13);
    Madd(s, 0, 0, 0, 13);
    FP(s, 0) = Single(std::sqrt(F(FP(s, 0))));
    Mul(s, 13, 14, 0); Mul(s, 12, 15, 0);
    Mul(s, 0, 16, 0);
    Add(s, 26, 13, 20); Add(s, 25, 12, 19);
    Add(s, 24, 0, 18);
    // The comparison above survives the intervening vector arithmetic.
    if (!s.cr6.eq)
    {
        Sub(s, 0, 26, 20);
        Divs(s, 9, 0, 31);
    }
    CmpF(s, 29, 30);
    if (!s.cr6.eq)
    {
        Sub(s, 0, 25, 19);
        Divs(s, 8, 0, 29);
    }
    CmpF(s, 28, 30);
    if (!s.cr6.eq)
    {
        Sub(s, 0, 24, 18);
        Divs(s, 7, 0, 28);
    }
    R(s, 11) = 0;
    Mov(s, 1, 30);
    CmpF(s, 9, 30);
    if (s.cr6.eq)
    {
        CmpF(s, 8, 30);
        if (s.cr6.eq)
        {
            CmpF(s, 7, 30);
            if (s.cr6.eq) { R(s, 11) = 1; goto candidate_done; }
        }
        CmpF(s, 9, 30);
        if (s.cr6.eq)
        {
            CmpF(s, 8, 30);
            if (s.cr6.eq)
            {
                CmpF(s, 7, 30);
                if (s.cr6.eq) { R(s, 11) = 1; goto candidate_done; }
                Mov(s, 1, 7);
            }
            else Mov(s, 1, 8);
        }
        else Mov(s, 1, 9);
    }
    else Mov(s, 1, 9);
    CmpF(s, 1, 30);
    if (!s.cr6.lt)
    {
        CmpF(s, 1, 17);
        if (!s.cr6.gt) R(s, 11) = 1;
    }
candidate_done:
    R(s, 11) = W(R(s, 11)) & 0xffu;
    CmpU(s, R(s, 11), 0u);
    if (s.cr6.eq) return;
    R(s, 3) = R(s, 30) + 96u;
    R(s, 5) = m.ReadU32(W(R(s, 29) + 8u));
    R(s, 11) = m.ReadU32(W(R(s, 3) + 24u));
    CmpU(s, R(s, 11), 0u);
    if (!s.cr6.eq)
    {
        R(s, 3) = W(R(s, 11));
        R(s, 11) = m.ReadU32(W(R(s, 3)));
        R(s, 11) = m.ReadU32(W(R(s, 11) + 268u));
        s.ctr = R(s, 11);
        s.lr = 0x82627b94u;
        native.CallVirtual(W(s.ctr) & ~3u, m, s);
        DisableFlush(native, s);
        Mov(s, 27, 1);
    }
    else
    {
        R(s, 7) = 0;
        R(s, 6) = 1;
        R(s, 5) = R(s, 1) + 80u;
        s.lr = 0x82627bacu;
        (void)object_curve_sample::Apply(0x822c73e8u, m, lower, s);
        Load(m, native, s, 27, sp + 80u);
    }
    Sub(s, 13, 22, 24); Sub(s, 0, 21, 26);
    Sub(s, 12, 23, 25);
    CmpF(s, 27, 30);
    Mul(s, 11, 13, 13);
    Madd(s, 11, 0, 0, 11);
    Madd(s, 11, 12, 12, 11);
    FP(s, 26) = Single(std::sqrt(F(FP(s, 11))));
    if (!s.cr6.gt) return;
    CmpF(s, 26, 27);
    if (s.cr6.gt) return;
    Mul(s, 11, 13, 29);
    R(s, 3) = R(s, 1) + 120u;
    Mul(s, 10, 0, 28);
    Load(m, native, s, 1, sp + 84u);
    Mul(s, 9, 12, 31);
    Msub(s, 12, 12, 28, 11); Store(m, s, 12, sp + 120u);
    Msub(s, 13, 13, 31, 10); Store(m, s, 13, sp + 124u);
    Msub(s, 0, 0, 29, 9); Store(m, s, 0, sp + 128u);
    s.lr = 0x82627c0cu;
    Normalize(m, native, s);
    Sub(s, 0, 27, 26);
    R(s, 3) = R(s, 30) + 124u;
    R(s, 5) = m.ReadU32(W(R(s, 29) + 8u));
    Divs(s, 1, 0, 27);
    s.lr = 0x82627c20u;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    Load(m, native, s, 0, R(s, 26) + 8u);
    Mul(s, 0, 1, 0);
    Load(m, native, s, 13, sp + 120u);
    Load(m, native, s, 12, sp + 124u);
    Load(m, native, s, 11, sp + 128u);
    Load(m, native, s, 10, R(s, 31) + 48u);
    Load(m, native, s, 9, R(s, 31) + 52u);
    Load(m, native, s, 8, R(s, 31) + 56u);
    Mul(s, 13, 13, 0); Mul(s, 12, 12, 0); Mul(s, 0, 11, 0);
    Load(m, native, s, 11, sp + 412u);
    Mul(s, 13, 13, 11); Mul(s, 12, 12, 11); Mul(s, 0, 0, 11);
    Add(s, 13, 13, 10); Store(m, s, 13, R(s, 31) + 48u);
    Add(s, 13, 12, 9); Store(m, s, 13, R(s, 31) + 52u);
    Add(s, 0, 0, 8); Store(m, s, 0, R(s, 31) + 56u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry == 0x8229f208u)
    {
        Normalize(memory, native, state);
        return true;
    }
    if (entry != 0x826276b8u) return false;
    LowerServices lower(native);
    Save(memory, native, state);
    R(state, 11) = 0xffffffff83320000ull;
    Store(memory, state, 1, R(state, 1) + 412u);
    R(state, 30) = R(state, 3);
    R(state, 29) = R(state, 4);
    R(state, 11) = memory.ReadU32(0x83315ea4u);
    CmpS(state, R(state, 11), 1u);
    if (!state.cr6.eq)
    {
        R(state, 3) = memory.ReadU32(W(R(state, 29) + 4u));
        state.lr = 0x826276f0u;
        (void)object_sample_accumulator::Apply(0x82607318u,
            memory, lower, state);
    }
    Transform(memory, native, state);
    PrepareDirection(memory, native, state);
    R(state, 11) = memory.ReadU32(W(R(state, 29) + 124u));
    R(state, 25) = memory.ReadU32(W(R(state, 29) + 56u));
    R(state, 27) = R(state, 11) - 1u;
    R(state, 24) = memory.ReadU32(W(R(state, 29) + 120u));
    R(state, 11) = memory.ReadU32(W(R(state, 29) + 60u));
    CmpS(state, R(state, 27), 0u);
    if (!state.cr6.lt)
    {
        R(state, 10) = std::uint64_t((W(R(state, 27)) << 1u) & 0xfffffffeu);
        const auto sp = W(R(state, 1));
        Load(memory, native, state, 16, sp + 128u);
        Load(memory, native, state, 15, sp + 124u);
        R(state, 28) = R(state, 10) + R(state, 11);
        Load(memory, native, state, 14, sp + 120u);
        R(state, 11) = 0xffffffff82190000ull;
        R(state, 26) = R(state, 11) - 27252u;
        R(state, 11) = 0xffffffff82000000ull;
        Load(memory, native, state, 17, R(state, 26));
        Load(memory, native, state, 30, R(state, 11) + 3664u);
        do
        {
            ApplyOne(memory, native, lower, state);
            R(state, 27) -= 1u;
            R(state, 28) -= 2u;
            CmpS(state, R(state, 27), 0u);
        } while (!state.cr6.lt);
    }
    Restore(memory, native, state);
    return true;
}
} // namespace lo::semantic::gpu::object_curve_record_displacement
