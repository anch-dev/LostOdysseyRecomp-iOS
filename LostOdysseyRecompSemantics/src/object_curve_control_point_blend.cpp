#include "lo_semantics/object_curve_control_point_blend.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_control_point_blend
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = object_child_float::Condition;

std::uint64_t& R(Registers& s, unsigned i) { return s.r[i]; }
std::uint64_t& FP(Registers& s, unsigned i)
{
    std::uint64_t* fields[] = {&s.f0_bits, &s.f1_bits, &s.f2_bits,
        &s.f3_bits, &s.f4_bits, &s.f5_bits, &s.f6_bits, &s.f7_bits,
        &s.f8_bits, &s.f9_bits, &s.f10_bits, &s.f11_bits, &s.f12_bits,
        &s.f13_bits, &s.f14_bits, &s.f15_bits, &s.f16_bits, &s.f17_bits,
        &s.f18_bits, &s.f19_bits, &s.f20_bits, &s.f21_bits, &s.f22_bits,
        &s.f23_bits, &s.f24_bits, &s.f25_bits, &s.f26_bits, &s.f27_bits,
        &s.f28_bits, &s.f29_bits, &s.f30_bits, &s.f31_bits};
    return *fields[i];
}
std::uint32_t W(std::uint64_t v) { return Address(v); }
std::int32_t S(std::uint64_t v)
{ return std::bit_cast<std::int32_t>(W(v)); }
double F(std::uint64_t v) { return std::bit_cast<double>(v); }
std::uint64_t Bits(double v) { return std::bit_cast<std::uint64_t>(v); }
std::uint64_t Single(double v) { return Bits(double(float(v))); }
void CmpS(Condition& c, Registers& s, std::uint64_t a, std::uint64_t b)
{ c = {std::uint8_t(S(a) < S(b)), std::uint8_t(S(a) > S(b)),
      std::uint8_t(S(a) == S(b)), s.xer_so}; }
void CmpU(Condition& c, Registers& s, std::uint64_t a, std::uint64_t b)
{ c = {std::uint8_t(W(a) < W(b)), std::uint8_t(W(a) > W(b)),
      std::uint8_t(W(a) == W(b)), s.xer_so}; }
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
    unsigned d, std::uint64_t address)
{
    DisableFlush(native, s);
    FP(s, d) = LoadedSingle::FromWord(m.ReadU32(W(address))).FprBits();
}
void Store(GuestMemory& m, Registers& s, unsigned source,
    std::uint64_t address)
{ m.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(FP(s, source))))); }
void Mul(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) * F(FP(s, b))); }
void Add(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) + F(FP(s, b))); }
void Sub(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) - F(FP(s, b))); }

void ZeroSix(GuestMemory& m, std::uint64_t address)
{
    for (unsigned i = 0; i < 3u; ++i)
        WriteU64(m, W(address + i * 8u), 0u);
}

void BlendSix(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    DisableFlush(native, s);
    WriteU64(m, sp - 8u, FP(s, 31));
    R(s, 11) = 0xffffffff82190000ull;
    Load(m, native, s, 13, R(s, 6));
    R(s, 8) = 0xffffffff82190000ull;
    Load(m, native, s, 9, R(s, 5));
    Load(m, native, s, 6, R(s, 3));
    R(s, 10) = R(s, 7);
    Load(m, native, s, 12, R(s, 4) + 4u);
    R(s, 9) = 6u;
    Load(m, native, s, 8, R(s, 5) + 4u);
    Load(m, native, s, 0, R(s, 11) - 27252u);
    R(s, 11) = R(s, 1) - 32u;
    Sub(s, 10, 0, 13);
    Load(m, native, s, 0, R(s, 8) - 26728u);
    Load(m, native, s, 13, R(s, 4));
    Load(m, native, s, 5, R(s, 3) + 4u);
    Load(m, native, s, 11, R(s, 4) + 8u);
    Load(m, native, s, 7, R(s, 5) + 8u);
    Load(m, native, s, 4, R(s, 3) + 8u);
    Load(m, native, s, 3, R(s, 5) + 12u);
    Load(m, native, s, 31, R(s, 3) + 12u);
    Load(m, native, s, 2, R(s, 5) + 16u);
    Load(m, native, s, 1, R(s, 5) + 20u);
    Mul(s, 0, 10, 0);
    Sub(s, 10, 9, 13);
    Sub(s, 13, 13, 6);
    Load(m, native, s, 6, R(s, 3) + 20u);
    Sub(s, 9, 8, 12);
    Sub(s, 12, 12, 5);
    Sub(s, 8, 7, 11);
    Load(m, native, s, 7, R(s, 3) + 16u);
    Sub(s, 11, 11, 4);
    Add(s, 13, 13, 10);
    Load(m, native, s, 10, R(s, 4) + 12u);
    Sub(s, 10, 10, 31);
    Add(s, 12, 12, 9);
    Load(m, native, s, 9, R(s, 4) + 16u);
    Sub(s, 9, 9, 7);
    Add(s, 11, 11, 8);
    Load(m, native, s, 8, R(s, 4) + 20u);
    Sub(s, 8, 8, 6);
    Mul(s, 13, 13, 0); Store(m, s, 13, sp - 48u);
    R(s, 8) = m.ReadU32(sp - 48u);
    Mul(s, 13, 12, 0); Store(m, s, 13, sp - 44u);
    Load(m, native, s, 12, R(s, 4) + 16u);
    Mul(s, 13, 11, 0); Store(m, s, 13, sp - 40u);
    Load(m, native, s, 13, R(s, 4) + 12u);
    Sub(s, 12, 2, 12);
    Sub(s, 13, 3, 13);
    m.WriteU32(sp - 32u, W(R(s, 8)));
    R(s, 8) = m.ReadU32(sp - 44u);
    Load(m, native, s, 11, R(s, 4) + 20u);
    Sub(s, 11, 1, 11);
    m.WriteU32(sp - 28u, W(R(s, 8)));
    R(s, 8) = m.ReadU32(sp - 40u);
    Add(s, 12, 9, 12);
    Add(s, 13, 10, 13);
    m.WriteU32(sp - 24u, W(R(s, 8)));
    Add(s, 11, 8, 11);
    Mul(s, 13, 13, 0); Store(m, s, 13, sp - 48u);
    R(s, 8) = m.ReadU32(sp - 48u);
    Mul(s, 13, 12, 0); Store(m, s, 13, sp - 44u);
    Mul(s, 0, 11, 0); Store(m, s, 0, sp - 40u);
    m.WriteU32(sp - 20u, W(R(s, 8)));
    R(s, 8) = m.ReadU32(sp - 44u);
    m.WriteU32(sp - 16u, W(R(s, 8)));
    R(s, 8) = m.ReadU32(sp - 40u);
    m.WriteU32(sp - 12u, W(R(s, 8)));
    s.ctr = R(s, 9);
    for (unsigned i = 0; i < 6u; ++i)
    {
        R(s, 9) = m.ReadU32(W(R(s, 11)));
        R(s, 11) += 4u;
        m.WriteU32(W(R(s, 10)), W(R(s, 9)));
        R(s, 10) += 4u;
        s.ctr -= 1u;
    }
    DisableFlush(native, s);
    FP(s, 31) = ReadU64(m, sp - 8u);
}

void Save27(GuestMemory& m, Registers& s)
{
    const auto old_sp = W(R(s, 1));
    R(s, 12) = s.lr;
    s.lr = 0x8262b618u;
    for (unsigned i = 27u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    R(s, 1) -= 192u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore27(GuestMemory& m, Registers& s)
{
    R(s, 1) += 192u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 27u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, old_sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(old_sp - 8u);
    s.lr = R(s, 12);
}

void Records(GuestMemory& m, NativeServices& native, Registers& s)
{
    Save27(m, s);
    R(s, 28) = R(s, 3);
    DisableFlush(native, s);
    Store(m, s, 1, R(s, 1) + 220u);
    R(s, 11) = 0xffffffff82000000ull;
    R(s, 27) = 0u;
    R(s, 29) = R(s, 27);
    R(s, 6) = m.ReadU32(W(R(s, 28) + 4u));
    Load(m, native, s, 0, R(s, 11) + 3664u);
    Store(m, s, 0, R(s, 1) + 220u);
    CmpS(s.cr6, s, R(s, 6), 0u);
    if (s.cr6.gt) do
    {
        R(s, 11) = recovery_abi::WordRotateMask(R(s, 29), 2, 0xfffffffcu);
        R(s, 7) = m.ReadU32(W(R(s, 28)));
        R(s, 9) = R(s, 1) + 80u;
        R(s, 11) = R(s, 29) + R(s, 11);
        R(s, 8) = 6u;
        R(s, 30) = recovery_abi::WordRotateMask(R(s, 11), 4, 0xfffffff0u);
        R(s, 11) = R(s, 7) + R(s, 30);
        R(s, 31) = R(s, 11) + 28u;
        R(s, 10) = R(s, 31);
        s.ctr = R(s, 8);
        for (unsigned i = 0; i < 6u; ++i)
        {
            R(s, 8) = m.ReadU32(W(R(s, 10)));
            R(s, 10) += 4u;
            m.WriteU32(W(R(s, 9)), W(R(s, 8)));
            R(s, 9) += 4u;
            s.ctr -= 1u;
        }
        R(s, 10) = R(s, 11) + 52u;
        R(s, 9) = R(s, 1) + 112u;
        R(s, 8) = 6u;
        s.ctr = R(s, 8);
        for (unsigned i = 0; i < 6u; ++i)
        {
            R(s, 8) = m.ReadU32(W(R(s, 10)));
            R(s, 10) += 4u;
            m.WriteU32(W(R(s, 9)), W(R(s, 8)));
            R(s, 9) += 4u;
            s.ctr -= 1u;
        }
        CmpS(s.cr6, s, R(s, 29), 0u);
        if (s.cr6.eq)
        {
            s.xer_ca = W(R(s, 6)) > 0u;
            R(s, 11) = R(s, 6) - 1u;
            CmpS(s.cr0, s, R(s, 11), 0u);
            if (!s.cr0.gt || m.ReadU8(W(R(s, 7) + 76u)) == 1u)
            {
                if (s.cr0.gt)
                {
                    R(s, 11) = m.ReadU8(W(R(s, 7) + 76u));
                    CmpU(s.cr6, s, R(s, 11), 1u);
                }
                R(s, 11) = R(s, 1) + 112u;
                ZeroSix(m, R(s, 11));
            }
            else
            {
                R(s, 11) = m.ReadU8(W(R(s, 7) + 76u));
                CmpU(s.cr6, s, R(s, 11), 1u);
            }
        }
        else
        {
            R(s, 10) = R(s, 6) - 1u;
            CmpS(s.cr6, s, R(s, 29), R(s, 10));
            if (!s.cr6.lt)
            {
                R(s, 11) = m.ReadU8(W(R(s, 11) + 76u));
                CmpU(s.cr6, s, R(s, 11), 1u);
                if (s.cr6.eq)
                {
                    R(s, 11) = R(s, 1) + 80u;
                    ZeroSix(m, R(s, 11));
                }
            }
            else
            {
                R(s, 10) = m.ReadU8(W(R(s, 11) + 76u));
                CmpU(s.cr6, s, R(s, 10), 1u);
                if (s.cr6.eq)
                {
                    R(s, 10) = m.ReadU8(W(R(s, 11) - 4u));
                    CmpU(s.cr6, s, R(s, 10), 1u);
                    if (!s.cr6.eq) CmpU(s.cr6, s, R(s, 10), 3u);
                    if (!s.cr6.eq) CmpU(s.cr6, s, R(s, 10), 4u);
                    if (s.cr6.eq)
                    {
                        R(s, 10) = m.ReadU8(W(R(s, 11) + 76u));
                        CmpU(s.cr6, s, R(s, 10), 1u);
                        if (!s.cr6.eq) CmpU(s.cr6, s, R(s, 10), 3u);
                        if (!s.cr6.eq) CmpU(s.cr6, s, R(s, 10), 4u);
                        if (s.cr6.eq)
                        {
                            R(s, 7) = R(s, 1) + 80u;
                            R(s, 6) = R(s, 1) + 220u;
                            R(s, 5) = R(s, 11) + 84u;
                            R(s, 4) = R(s, 11) + 4u;
                            R(s, 3) = R(s, 11) - 76u;
                            s.lr = 0x8262b750u;
                            BlendSix(m, native, s);
                            R(s, 10) = R(s, 1) + 112u;
                            R(s, 11) = R(s, 1) + 80u;
                            R(s, 9) = 6u;
                            s.ctr = R(s, 9);
                            for (unsigned i = 0; i < 6u; ++i)
                            {
                                R(s, 9) = m.ReadU32(W(R(s, 11)));
                                R(s, 11) += 4u;
                                m.WriteU32(W(R(s, 10)), W(R(s, 9)));
                                R(s, 10) += 4u;
                                s.ctr -= 1u;
                            }
                        }
                    }
                    if (!s.cr6.eq)
                    {
                        R(s, 11) = m.ReadU8(W(R(s, 11) - 4u));
                        CmpU(s.cr6, s, R(s, 11), 2u);
                        if (s.cr6.eq)
                        {
                            R(s, 11) = R(s, 1) + 80u;
                            R(s, 10) = R(s, 1) + 112u;
                            ZeroSix(m, R(s, 11));
                            ZeroSix(m, R(s, 10));
                        }
                    }
                }
            }
        }
        R(s, 11) = R(s, 1) + 80u;
        R(s, 10) = R(s, 31);
        R(s, 9) = 6u;
        s.ctr = R(s, 9);
        for (unsigned i = 0; i < 6u; ++i)
        {
            R(s, 9) = m.ReadU32(W(R(s, 11)));
            R(s, 11) += 4u;
            m.WriteU32(W(R(s, 10)), W(R(s, 9)));
            R(s, 10) += 4u;
            s.ctr -= 1u;
        }
        R(s, 10) = m.ReadU32(W(R(s, 28)));
        R(s, 11) = R(s, 1) + 112u;
        R(s, 9) = 6u;
        R(s, 10) = R(s, 30) + R(s, 10);
        R(s, 10) += 52u;
        s.ctr = R(s, 9);
        for (unsigned i = 0; i < 6u; ++i)
        {
            R(s, 9) = m.ReadU32(W(R(s, 11)));
            R(s, 11) += 4u;
            m.WriteU32(W(R(s, 10)), W(R(s, 9)));
            R(s, 10) += 4u;
            s.ctr -= 1u;
        }
        R(s, 6) = m.ReadU32(W(R(s, 28) + 4u));
        R(s, 29) += 1u;
        CmpS(s.cr6, s, R(s, 29), R(s, 6));
    } while (s.cr6.lt);
    Restore27(m, s);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry == 0x8262c4b0u) { BlendSix(memory, native, state); return true; }
    if (entry == 0x8262b610u) { Records(memory, native, state); return true; }
    return false;
}
} // namespace lo::semantic::gpu::object_curve_control_point_blend
