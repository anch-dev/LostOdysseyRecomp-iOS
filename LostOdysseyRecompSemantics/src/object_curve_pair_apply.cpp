#include "lo_semantics/object_curve_pair_apply.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_pair_apply
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

void CmpS(Registers& s, std::uint64_t x, std::uint64_t y)
{
    s.cr6 = {std::uint8_t(S(x) < S(y)), std::uint8_t(S(x) > S(y)),
        std::uint8_t(S(x) == S(y)), s.xer_so};
}
void CmpU(Registers& s, std::uint64_t x, std::uint64_t y)
{
    s.cr6 = {std::uint8_t(W(x) < W(y)), std::uint8_t(W(x) > W(y)),
        std::uint8_t(W(x) == W(y)), s.xer_so};
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
std::uint64_t LoadF(GuestMemory& m, NativeServices& native,
    Registers& s, std::uint64_t address)
{
    DisableFlush(native, s);
    return LoadedSingle::FromWord(m.ReadU32(W(address))).FprBits();
}
void StoreF(GuestMemory& m, std::uint64_t address, std::uint64_t bits)
{ m.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(bits)))); }

void Save25(GuestMemory& m, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x822ceab8u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    R(s, 1) -= 208u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore25(GuestMemory& m, Registers& s)
{
    R(s, 1) += 208u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 25u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
    s.lr = R(s, 12);
}
void SampleVector(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    m.WriteU32(W(R(s, 1) - 8u), W(R(s, 12)));
    WriteU64(m, W(R(s, 1) - 16u), R(s, 31));
    const auto old_sp = W(R(s, 1));
    R(s, 1) -= 112u;
    m.WriteU32(W(R(s, 1)), old_sp);
    R(s, 11) = m.ReadU32(W(R(s, 4) + 24u));
    R(s, 31) = R(s, 3);
    CmpU(s, R(s, 11), 0);
    if (!s.cr6.eq)
    {
        R(s, 4) = W(R(s, 11));
        R(s, 11) = m.ReadU32(W(R(s, 4)));
        R(s, 11) = m.ReadU32(W(R(s, 11) + 268u));
        s.ctr = R(s, 11);
        s.lr = 0x822c7fecu;
        native.CallVirtual(W(s.ctr) & ~3u, m, s);
    }
    else
    {
        R(s, 7) = 0;
        R(s, 6) = 3;
        R(s, 5) = R(s, 1) + 80u;
        R(s, 3) = R(s, 4);
        s.lr = 0x822c8004u;
        (void)object_curve_sample::Apply(0x822c73e8u, m, native, s);
        R(s, 11) = m.ReadU32(W(R(s, 1) + 80u));
        R(s, 10) = m.ReadU32(W(R(s, 1) + 84u));
        R(s, 9) = m.ReadU32(W(R(s, 1) + 88u));
        m.WriteU32(W(R(s, 31)), W(R(s, 11)));
        m.WriteU32(W(R(s, 31) + 4u), W(R(s, 10)));
        m.WriteU32(W(R(s, 31) + 8u), W(R(s, 9)));
    }
    R(s, 3) = R(s, 31);
    R(s, 1) += 112u;
    R(s, 12) = m.ReadU32(W(R(s, 1) - 8u));
    s.lr = R(s, 12);
    R(s, 31) = ReadU64(m, W(R(s, 1) - 16u));
}

void SampleScalar(GuestMemory& m, NativeServices& native,
    Registers& s, std::uint32_t output)
{
    if (!s.cr6.eq)
    {
        R(s, 3) = m.ReadU32(W(R(s, 11) + 24u));
        R(s, 11) = m.ReadU32(W(R(s, 3)));
        R(s, 11) = m.ReadU32(W(R(s, 11) + 268u));
        s.ctr = R(s, 11);
        s.lr = output == 80u ? 0x822ceb70u : 0x822cebe0u;
        native.CallVirtual(W(s.ctr) & ~3u, m, s);
    }
    else
    {
        R(s, 7) = 0;
        R(s, 6) = 1;
        R(s, 5) = R(s, 1) + output;
        R(s, 3) = R(s, 11);
        s.lr = output == 80u ? 0x822ceb88u : 0x822cebf8u;
        (void)object_curve_sample::Apply(0x822c73e8u, m, native, s);
        s.f1_bits = LoadF(m, native, s, R(s, 1) + output);
    }
}

void UpdateRecords(GuestMemory& m, NativeServices& native, Registers& s)
{
    Save25(m, s);
    R(s, 31) = R(s, 4);
    R(s, 29) = R(s, 3);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 124u));
    R(s, 26) = m.ReadU32(W(R(s, 31) + 56u));
    R(s, 27) = R(s, 11) - 1u;
    R(s, 25) = m.ReadU32(W(R(s, 31) + 120u));
    R(s, 11) = m.ReadU32(W(R(s, 31) + 60u));
    CmpS(s, R(s, 27), 0);
    if (s.cr6.lt)
    {
        Restore25(m, s);
        return;
    }
    R(s, 10) = (std::uint64_t(W(R(s, 27))) << 1u) & 0xfffffffeu;
    R(s, 28) = R(s, 10) + R(s, 11);
    for (;;)
    {
        R(s, 11) = m.ReadU16(W(R(s, 28)));
        R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
            std::int64_t(S(R(s, 25))));
        R(s, 30) = R(s, 11) + R(s, 26);
        R(s, 11) = m.ReadU32(W(R(s, 30) + 92u));
        R(s, 11) = W(R(s, 11)) & 1u;
        CmpS(s, R(s, 11), 0);
        if (s.cr6.eq)
        {
            R(s, 11) = m.ReadU32(W(R(s, 29) + 124u));
            R(s, 7) = 0;
            R(s, 6) = m.ReadU32(W(R(s, 31) + 8u));
            R(s, 4) = R(s, 29) + 68u;
            R(s, 11) = W(R(s, 11)) & 0x80000000u;
            CmpU(s, R(s, 11), 0);
            const bool upper = !s.cr6.eq;
            R(s, 3) = R(s, 1) + (upper ? 104u : 120u);
            s.f1_bits = LoadF(m, native, s,
                upper ? R(s, 31) + 140u : R(s, 30) + 12u);
            s.lr = upper ? 0x822ceb2cu : 0x822ceb9cu;
            SampleVector(m, native, s);

            R(s, 11) = R(s, 29) + 96u;
            R(s, 9) = m.ReadU32(W(R(s, 3)));
            s.f1_bits = LoadF(m, native, s,
                upper ? R(s, 31) + 140u : R(s, 30) + 12u);
            R(s, 5) = m.ReadU32(W(R(s, 31) + 8u));
            R(s, 10) = m.ReadU32(W(R(s, 11) + 24u));
            m.WriteU32(W(R(s, 1) + 88u), W(R(s, 9)));
            CmpU(s, R(s, 10), 0);
            R(s, 9) = m.ReadU32(W(R(s, 3) + 4u));
            R(s, 10) = m.ReadU32(W(R(s, 3) + 8u));
            m.WriteU32(W(R(s, 1) + 92u), W(R(s, 9)));
            m.WriteU32(W(R(s, 1) + 96u), W(R(s, 10)));
            SampleScalar(m, native, s, upper ? 80u : 84u);

            s.f0_bits = LoadF(m, native, s, R(s, 30) + 96u);
            s.f13_bits = LoadF(m, native, s, R(s, 30) + 100u);
            s.f12_bits = LoadF(m, native, s, R(s, 1) + 88u);
            s.f11_bits = LoadF(m, native, s, R(s, 1) + 92u);
            s.f0_bits = Single(F(s.f0_bits) * F(s.f12_bits));
            s.f13_bits = Single(F(s.f13_bits) * F(s.f11_bits));
            s.f12_bits = LoadF(m, native, s, R(s, 30) + 104u);
            s.f11_bits = LoadF(m, native, s, R(s, 30) + 108u);
            s.f10_bits = LoadF(m, native, s, R(s, 1) + 96u);
            s.f11_bits = Single(F(s.f11_bits) * F(s.f1_bits));
            s.f12_bits = Single(F(s.f12_bits) * F(s.f10_bits));
            StoreF(m, R(s, 30) + 96u, s.f0_bits);
            StoreF(m, R(s, 30) + 100u, s.f13_bits);
            StoreF(m, R(s, 30) + 104u, s.f12_bits);
            StoreF(m, R(s, 30) + 108u, s.f11_bits);
        }
        R(s, 27) -= 1u;
        R(s, 28) -= 2u;
        CmpS(s, R(s, 27), 0);
        if (s.cr6.lt) break;
    }
    Restore25(m, s);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry == 0x822c7fb8u)
    {
        SampleVector(memory, native, state);
        return true;
    }
    if (entry == 0x822ceab0u)
    {
        UpdateRecords(memory, native, state);
        return true;
    }
    return false;
}
} // namespace lo::semantic::gpu::object_curve_pair_apply
