#include "lo_semantics/object_curve_sample.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_sample
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
std::uint64_t& R(Registers& s, unsigned i) { return s.r[i]; }
std::uint32_t W(std::uint64_t v) { return Address(v); }
std::int32_t S(std::uint64_t v)
{ return std::bit_cast<std::int32_t>(W(v)); }
double F(std::uint64_t v) { return std::bit_cast<double>(v); }
std::uint64_t Bits(double v) { return std::bit_cast<std::uint64_t>(v); }
std::uint64_t Single(double v) { return Bits(double(float(v))); }
void CmpU(Registers& s, std::uint64_t x, std::uint64_t y)
{
    s.cr6 = {std::uint8_t(W(x) < W(y)), std::uint8_t(W(x) > W(y)),
        std::uint8_t(W(x) == W(y)), s.xer_so};
}
void CmpS(Registers& s, std::uint64_t x, std::uint64_t y)
{
    s.cr6 = {std::uint8_t(S(x) < S(y)), std::uint8_t(S(x) > S(y)),
        std::uint8_t(S(x) == S(y)), s.xer_so};
}
void CmpF(Registers& s, std::uint64_t x, std::uint64_t y)
{
    const auto a = F(x), b = F(y);
    const bool un = std::isnan(a) || std::isnan(b);
    s.cr6 = {std::uint8_t(!un && a < b), std::uint8_t(!un && a > b),
        std::uint8_t(!un && a == b), std::uint8_t(un)};
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
std::uint64_t LoadF(GuestMemory& memory, NativeServices& native,
    Registers& s, std::uint64_t address)
{
    DisableFlush(native, s);
    return LoadedSingle::FromWord(memory.ReadU32(W(address))).FprBits();
}
void StoreF(GuestMemory& memory, std::uint64_t address, std::uint64_t bits)
{
    memory.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(bits))));
}
std::uint64_t RotateMask(std::uint64_t value, unsigned shift,
    std::uint64_t mask)
{
    const auto repeated = std::uint64_t(W(value)) | (value << 32u);
    return std::rotl(repeated, int(shift)) & mask;
}
std::uint64_t ConvertWord(double value)
{
    if (value > double(INT_MAX)) return std::uint64_t(INT_MAX);
    if (!std::isfinite(value) || value < double(INT_MIN))
        return std::uint64_t(std::int64_t(INT_MIN));
    return std::uint64_t(std::int64_t(std::trunc(value)));
}

void FindSampleSpan(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 11) = 0;
    s.f0_bits = LoadF(m, native, s, R(s, 3) + 20u);
    CmpF(s, s.f1_bits, s.f0_bits);
    m.WriteU32(W(R(s, 6)), W(R(s, 11)));
    if (!s.cr6.gt) goto fallback;
    R(s, 11) = 0xffffffff82000000ull;
    s.f13_bits = LoadF(m, native, s, R(s, 3) + 16u);
    s.f12_bits = LoadF(m, native, s, R(s, 11) + 3664u);
    CmpF(s, s.f13_bits, s.f12_bits);
    if (s.cr6.eq) goto fallback;
    s.f0_bits = Single(F(s.f1_bits) - F(s.f0_bits));
    R(s, 9) = m.ReadU8(W(R(s, 3) + 2u));
    R(s, 10) = m.ReadU8(W(R(s, 3) + 3u));
    R(s, 8) = R(s, 1) - 16u;
    R(s, 11) = m.ReadU32(W(R(s, 3) + 8u));
    R(s, 10) = std::uint64_t(std::int64_t(S(R(s, 10))) *
        std::int64_t(S(R(s, 9))));
    s.f13_bits = Single(F(s.f0_bits) / F(s.f13_bits));
    R(s, 11) -= 2u;
    R(s, 11) = (R(s, 11) & 0xffffffff00000000ull) |
        W(std::uint64_t(S(R(s, 11)) / S(R(s, 10))));
    R(s, 9) = R(s, 11) - 1u;
    s.f13_bits = ConvertWord(F(s.f13_bits));
    m.WriteU32(W(R(s, 8)), W(s.f13_bits));
    R(s, 11) = m.ReadU32(W(R(s, 1) - 16u));
    CmpS(s, R(s, 11), R(s, 9));
    if (!s.cr6.lt)
    {
        R(s, 11) = R(s, 9);
        goto store_base;
    }
    R(s, 4) = std::uint64_t(std::int64_t(S(R(s, 11))));
    s.f13_bits = LoadF(m, native, s, R(s, 3) + 16u);
    R(s, 9) = R(s, 11) + 1u;
    R(s, 8) = m.ReadU32(W(R(s, 3) + 4u));
    R(s, 9) = std::uint64_t(std::int64_t(S(R(s, 9))) *
        std::int64_t(S(R(s, 10))));
    WriteU64(m, W(R(s, 1) - 16u), R(s, 4));
    s.f12_bits = ReadU64(m, W(R(s, 1) - 16u));
    s.f12_bits = Bits(double(std::bit_cast<std::int64_t>(s.f12_bits)));
    R(s, 9) += 2u;
    R(s, 9) = RotateMask(R(s, 9), 2u, 0xfffffffcu);
    s.f12_bits = Single(F(s.f12_bits));
    R(s, 9) += R(s, 8);
    m.WriteU32(W(R(s, 6)), W(R(s, 9)));
    s.f0_bits = Single(-(F(s.f12_bits) * F(s.f13_bits) - F(s.f0_bits)));
    s.f0_bits = Single(F(s.f0_bits) / F(s.f13_bits));
    StoreF(m, R(s, 7), s.f0_bits);
store_base:
    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 10))) *
        std::int64_t(S(R(s, 11))));
    R(s, 10) = m.ReadU32(W(R(s, 3) + 4u));
    R(s, 11) += 2u;
    R(s, 11) = RotateMask(R(s, 11), 2u, 0xfffffffcu);
    R(s, 11) += R(s, 10);
    m.WriteU32(W(R(s, 5)), W(R(s, 11)));
    return;
fallback:
    R(s, 11) = m.ReadU32(W(R(s, 3) + 4u));
    R(s, 11) += 8u;
    m.WriteU32(W(R(s, 5)), W(R(s, 11)));
}

void Save(GuestMemory& m, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x822c73f0u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 27u; i <= 31u; ++i)
        WriteU64(m, sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(sp - 8u, W(R(s, 12)));
    R(s, 1) -= 192u;
    m.WriteU32(W(R(s, 1)), sp);
}
void Restore(GuestMemory& m, Registers& s)
{
    R(s, 1) += 192u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 27u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
    s.lr = R(s, 12);
}
void NextRandom(GuestMemory& m, Registers& s, unsigned index)
{
    const auto global = W(R(s, 4) + 13948u);
    R(s, index) = m.ReadU32(global);
    R(s, index) = std::uint64_t(std::int64_t(S(R(s, index))) *
        std::int64_t(S(R(s, 30))));
    R(s, index) += R(s, 31);
    m.WriteU32(global, W(R(s, index)));
    R(s, index) = W(R(s, index)) & 0x7fffffu;
    R(s, index) |= 0x3f800000u;
    m.WriteU32(W(R(s, 1) + 80u), W(R(s, index)));
}
void RandomizeLane(GuestMemory& m, NativeServices& native,
    Registers& s, unsigned random_register, std::uint64_t output,
    unsigned scratch32, unsigned scratch64)
{
    NextRandom(m, s, random_register);
    s.f0_bits = LoadF(m, native, s, output);
    s.f11_bits = Single(F(s.f13_bits) - F(s.f0_bits));
    s.f13_bits = LoadF(m, native, s, R(s, 1) + 80u);
    s.f10_bits = ConvertWord(F(s.f13_bits));
    R(s, 7) = R(s, 1) + scratch32;
    m.WriteU32(W(R(s, 7)), W(s.f10_bits));
    R(s, random_register) = m.ReadU32(W(R(s, 1) + scratch32));
    R(s, random_register) =
        std::uint64_t(std::int64_t(S(R(s, random_register))));
    WriteU64(m, W(R(s, 1) + scratch64), R(s, random_register));
    s.f10_bits = ReadU64(m, W(R(s, 1) + scratch64));
    s.f10_bits = Single(double(std::bit_cast<std::int64_t>(s.f10_bits)));
    s.f13_bits = Single(F(s.f13_bits) - F(s.f10_bits));
    s.f0_bits = Single(std::fma(F(s.f13_bits), F(s.f11_bits), F(s.f0_bits)));
    StoreF(m, output, s.f0_bits);
}
void FourLane(GuestMemory& m, NativeServices& native, Registers& s,
    unsigned lane, std::uint64_t output)
{
    R(s, 8) = m.ReadU8(W(R(s, 3) + 3u));
    CmpU(s, R(s, 6), 0);
    R(s, 8) = std::uint64_t(std::int64_t(S(R(s, 8))) *
        std::int64_t(S(R(s, 5))));
    R(s, 8) += R(s, 11);
    R(s, 8) += lane;
    R(s, 8) = RotateMask(R(s, 8), 2u, 0xfffffffcu);
    s.f0_bits = LoadF(m, native, s, R(s, 10) + R(s, 8));
    if (!s.cr6.eq)
    {
        s.f13_bits = LoadF(m, native, s, R(s, 6) + R(s, 8));
        s.f13_bits = Single(F(s.f13_bits) - F(s.f0_bits));
        s.f0_bits = Single(std::fma(F(s.f13_bits), F(s.f12_bits),
            F(s.f0_bits)));
    }
    StoreF(m, output, s.f0_bits);
    R(s, 8) = m.ReadU8(W(R(s, 3) + 1u));
    CmpU(s, R(s, 8), 2);
    if (!s.cr6.eq) return;
    R(s, 8) = m.ReadU8(W(R(s, 3) + 3u));
    CmpU(s, R(s, 6), 0);
    R(s, 8) += R(s, 11);
    R(s, 8) += lane;
    R(s, 8) = RotateMask(R(s, 8), 2u, 0xfffffffcu);
    if (s.cr6.eq)
        s.f13_bits = LoadF(m, native, s, R(s, 10) + R(s, 8));
    else
    {
        s.f0_bits = LoadF(m, native, s, R(s, 10) + R(s, 8));
        s.f13_bits = LoadF(m, native, s, R(s, 6) + R(s, 8));
        s.f13_bits = Single(F(s.f13_bits) - F(s.f0_bits));
        s.f13_bits = Single(std::fma(F(s.f13_bits), F(s.f12_bits),
            F(s.f0_bits)));
    }
    constexpr unsigned scratch32[] = {84u, 88u, 96u, 104u};
    RandomizeLane(m, native, s, 8u, output,
        scratch32[lane], 112u + lane * 8u);
}
void TailLane(GuestMemory& m, NativeServices& native,
    Registers& s, std::uint64_t output)
{
    R(s, 9) = m.ReadU8(W(R(s, 3) + 3u));
    CmpU(s, R(s, 6), 0);
    R(s, 9) = std::uint64_t(std::int64_t(S(R(s, 9))) *
        std::int64_t(S(R(s, 5))));
    R(s, 9) += R(s, 11);
    R(s, 9) = RotateMask(R(s, 9), 2u, 0xfffffffcu);
    s.f0_bits = LoadF(m, native, s, R(s, 10) + R(s, 9));
    if (!s.cr6.eq)
    {
        s.f13_bits = LoadF(m, native, s, R(s, 6) + R(s, 9));
        s.f13_bits = Single(F(s.f13_bits) - F(s.f0_bits));
        s.f0_bits = Single(std::fma(F(s.f13_bits), F(s.f12_bits),
            F(s.f0_bits)));
    }
    StoreF(m, output, s.f0_bits);
    R(s, 9) = m.ReadU8(W(R(s, 3) + 1u));
    CmpU(s, R(s, 9), 2);
    if (!s.cr6.eq) return;
    R(s, 9) = m.ReadU8(W(R(s, 3) + 3u));
    CmpU(s, R(s, 6), 0);
    R(s, 9) += R(s, 11);
    R(s, 9) = RotateMask(R(s, 9), 2u, 0xfffffffcu);
    if (s.cr6.eq)
        s.f13_bits = LoadF(m, native, s, R(s, 10) + R(s, 9));
    else
    {
        s.f0_bits = LoadF(m, native, s, R(s, 10) + R(s, 9));
        s.f13_bits = LoadF(m, native, s, R(s, 6) + R(s, 9));
        s.f13_bits = Single(F(s.f13_bits) - F(s.f0_bits));
        s.f13_bits = Single(std::fma(F(s.f13_bits), F(s.f12_bits),
            F(s.f0_bits)));
    }
    RandomizeLane(m, native, s, 9u, output, 104u, 136u);
}
void UpdateCurve(GuestMemory& m, NativeServices& native, Registers& s)
{
    Save(m, s);
    R(s, 11) = 0xffffffff82000000ull;
    R(s, 28) = R(s, 5);
    R(s, 27) = R(s, 6);
    R(s, 7) = R(s, 1) + 104u;
    R(s, 6) = R(s, 1) + 88u;
    R(s, 5) = R(s, 1) + 84u;
    s.f0_bits = LoadF(m, native, s, R(s, 11) + 3664u);
    StoreF(m, R(s, 1) + 104u, s.f0_bits);
    s.lr = 0x822c7418u;
    FindSampleSpan(m, native, s);

    R(s, 10) = 196280320u;
    R(s, 11) = m.ReadU8(W(R(s, 3) + 1u));
    R(s, 5) = 0;
    R(s, 30) = R(s, 10) | 33845u;
    R(s, 10) = 907608064u;
    CmpU(s, R(s, 11), 3u);
    R(s, 31) = R(s, 10) | 25451u;
    R(s, 4) = 0xffffffff83310000ull;
    if (s.cr6.eq)
    {
        R(s, 11) = m.ReadU32(W(R(s, 4) + 13948u));
        R(s, 10) = R(s, 1) + 80u;
        R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
            std::int64_t(S(R(s, 30))));
        R(s, 11) += R(s, 31);
        m.WriteU32(W(R(s, 4) + 13948u), W(R(s, 11)));
        R(s, 11) = (W(R(s, 11)) & 0x7fffffu) | 0x3f800000u;
        m.WriteU32(W(R(s, 1) + 96u), W(R(s, 11)));
        s.f0_bits = LoadF(m, native, s, R(s, 1) + 96u);
        s.f13_bits = ConvertWord(F(s.f0_bits));
        m.WriteU32(W(R(s, 10)), W(s.f13_bits));
        R(s, 11) = m.ReadU32(W(R(s, 1) + 80u));
        R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))));
        WriteU64(m, W(R(s, 1) + 96u), R(s, 11));
        R(s, 11) = 0xffffffff82190000ull;
        s.f13_bits = ReadU64(m, W(R(s, 1) + 96u));
        s.f13_bits = Single(double(std::bit_cast<std::int64_t>(s.f13_bits)));
        s.f13_bits = Single(F(s.f0_bits) - F(s.f13_bits));
        s.f0_bits = LoadF(m, native, s, R(s, 11) - 26728u);
        CmpF(s, s.f13_bits, s.f0_bits);
        if (s.cr6.gt) R(s, 5) = 1;
    }
    s.f12_bits = LoadF(m, native, s, R(s, 1) + 104u);
    R(s, 6) = m.ReadU32(W(R(s, 1) + 88u));
    R(s, 10) = m.ReadU32(W(R(s, 1) + 84u));
    R(s, 11) = 0;
    CmpS(s, R(s, 27), 4u);
    if (!s.cr6.lt)
    {
        R(s, 29) = R(s, 27) - 3u;
        R(s, 9) = R(s, 28) + 8u;
        do
        {
            FourLane(m, native, s, 0u, R(s, 9) - 8u);
            FourLane(m, native, s, 1u, R(s, 9) - 4u);
            FourLane(m, native, s, 2u, R(s, 9));
            FourLane(m, native, s, 3u, R(s, 9) + 4u);
            R(s, 11) += 4u;
            R(s, 9) += 16u;
            CmpS(s, R(s, 11), R(s, 29));
        } while (s.cr6.lt);
    }
    CmpS(s, R(s, 11), R(s, 27));
    if (s.cr6.lt)
    {
        R(s, 9) = RotateMask(R(s, 11), 2u, 0xfffffffcu);
        R(s, 8) = R(s, 9) + R(s, 28);
        do
        {
            TailLane(m, native, s, R(s, 8));
            R(s, 11) += 1u;
            R(s, 8) += 4u;
            CmpS(s, R(s, 11), R(s, 27));
        } while (s.cr6.lt);
    }
    Restore(m, s);
}
void SampleCurve(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    m.WriteU32(W(R(s, 1) - 8u), W(R(s, 12)));
    const auto old_sp = W(R(s, 1));
    R(s, 1) -= 96u;
    m.WriteU32(W(R(s, 1)), old_sp);
    R(s, 11) = m.ReadU32(W(R(s, 3) + 24u));
    CmpU(s, R(s, 11), 0u);
    if (!s.cr6.eq)
    {
        R(s, 3) = W(R(s, 11));
        R(s, 11) = m.ReadU32(W(R(s, 3)));
        R(s, 11) = m.ReadU32(W(R(s, 11) + 268u));
        s.ctr = R(s, 11);
        s.lr = 0x822c73b4u;
        native.CallVirtual(W(s.ctr) & ~3u, m, s);
    }
    else
    {
        R(s, 7) = 0;
        R(s, 6) = 1;
        R(s, 5) = R(s, 1) + 80u;
        s.lr = 0x822c73d4u;
        UpdateCurve(m, native, s);
        s.f1_bits = LoadF(m, native, s, R(s, 1) + 80u);
    }
    R(s, 1) += 96u;
    R(s, 12) = m.ReadU32(W(R(s, 1) - 8u));
    s.lr = R(s, 12);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry == 0x822c7388u)
    {
        SampleCurve(memory, native, state);
        return true;
    }
    if (entry == 0x822c73e8u)
    {
        UpdateCurve(memory, native, state);
        return true;
    }
    if (entry == 0x822c78d8u)
    {
        FindSampleSpan(memory, native, state);
        return true;
    }
    return false;
}

} // namespace lo::semantic::gpu::object_curve_sample
