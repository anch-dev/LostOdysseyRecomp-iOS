#include "lo_semantics/object_sample_accumulator.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_sample_accumulator
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
std::uint64_t RotateMask(std::uint64_t value, unsigned shift,
    std::uint64_t mask)
{
    return std::rotl(std::uint64_t(W(value)) | (value << 32u),
        int(shift)) & mask;
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
std::uint64_t LoadF(GuestMemory& m, NativeServices& native,
    Registers& s, std::uint64_t address)
{
    DisableFlush(native, s);
    return LoadedSingle::FromWord(m.ReadU32(W(address))).FprBits();
}
void StoreF(GuestMemory& m, NativeServices& native, Registers& s,
    std::uint64_t address, std::uint64_t bits)
{
    DisableFlush(native, s);
    m.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(bits))));
}
std::uint64_t ConvertWord(double value)
{
    if (value > double(INT_MAX)) return std::uint64_t(INT_MAX);
    if (!std::isfinite(value) || value < double(INT_MIN))
        return std::uint64_t(std::int64_t(INT_MIN));
    return std::uint64_t(std::int64_t(std::trunc(value)));
}
void Save25(GuestMemory& m, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x822c79b8u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(m, sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(sp - 8u, W(R(s, 12)));
    R(s, 1) -= 192u;
    m.WriteU32(W(R(s, 1)), sp);
}
void Restore25(GuestMemory& m, Registers& s)
{
    R(s, 1) += 192u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 25u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
    s.lr = R(s, 12);
}

void SelectSample(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 11) = m.ReadU32(W(R(s, 3) + 220u));
    R(s, 10) = 0;
    R(s, 7) = 0;
    R(s, 6) = R(s, 11) - 1u;
    CmpS(s, R(s, 6), 0);
    if (s.cr6.gt)
    {
        R(s, 10) = m.ReadU32(W(R(s, 4) + 8u));
        R(s, 11) = m.ReadU32(W(R(s, 3) + 216u));
        R(s, 8) = m.ReadU32(W(R(s, 10) + 720u));
        do
        {
            R(s, 9) = m.ReadU32(W(R(s, 11)));
            R(s, 10) = m.ReadU32(W(R(s, 11) + 4u));
            R(s, 5) = m.ReadU32(W(R(s, 9) + 64u));
            CmpS(s, R(s, 5), R(s, 8));
            if (!s.cr6.gt)
            {
                R(s, 5) = m.ReadU32(W(R(s, 10) + 64u));
                CmpS(s, R(s, 5), R(s, 8));
                if (s.cr6.gt)
                {
                    R(s, 10) = R(s, 9);
                    break;
                }
            }
            R(s, 7) += 1u;
            R(s, 11) += 4u;
            CmpS(s, R(s, 7), R(s, 6));
        } while (s.cr6.lt);
    }
    R(s, 11) = m.ReadU32(W(R(s, 10) + 60u));
    R(s, 9) = m.ReadU32(W(R(s, 4) + 220u));
    m.WriteU32(W(R(s, 4) + 16u), W(R(s, 10)));
    m.WriteU32(W(R(s, 4) + 12u), W(R(s, 11)));
    R(s, 11) = W(R(s, 11));
    R(s, 11) = RotateMask(R(s, 11), 2u, 0xfffffffcu);
    s.f0_bits = LoadF(m, native, s, R(s, 11) + R(s, 9));
    StoreF(m, native, s, R(s, 4) + 216u, s.f0_bits);
}

void Accumulate(GuestMemory& m, NativeServices& native, Registers& s)
{
    Save25(m, s);
    R(s, 11) = 0xffffffff82000000ull;
    R(s, 31) = R(s, 3);
    R(s, 28) = R(s, 4);
    R(s, 30) = R(s, 5);
    s.f1_bits = LoadF(m, native, s, R(s, 11) + 3664u);
    R(s, 11) = 0xffffffff83310000ull;
    R(s, 11) = m.ReadU32(W(R(s, 11) + 24228u));
    CmpS(s, R(s, 11), 1);
    if (!s.cr6.eq)
    {
        R(s, 4) = R(s, 31);
        R(s, 3) = m.ReadU32(W(R(s, 31) + 4u));
        s.lr = 0x822c79ecu;
        SelectSample(m, native, s);
    }
    R(s, 7) = m.ReadU32(W(R(s, 31) + 16u));
    R(s, 11) = m.ReadU32(W(R(s, 7) + 72u));
    R(s, 10) = m.ReadU32(W(R(s, 11) + 124u));
    CmpS(s, R(s, 10), 0);
    if (!s.cr6.gt)
    {
        Restore25(m, s);
        return;
    }
    R(s, 9) = 196280320u;
    R(s, 10) = 0xffffffff82000000ull;
    R(s, 3) = R(s, 9) | 33845u;
    R(s, 9) = 907608064u;
    R(s, 27) = 0;
    R(s, 8) = 0;
    s.f13_bits = LoadF(m, native, s, R(s, 10) + 3472u);
    R(s, 6) = 0;
    R(s, 5) = 0xffffffff83310000ull;
    R(s, 4) = R(s, 9) | 25451u;
    R(s, 29) = 1;
    for (;;)
    {
        R(s, 10) = m.ReadU32(W(R(s, 7) + 60u));
        R(s, 26) = m.ReadU32(W(R(s, 31) + 184u));
        R(s, 9) = RotateMask(R(s, 10), 1u, 0xfffffffeu);
        R(s, 11) = m.ReadU32(W(R(s, 11) + 120u));
        R(s, 10) += R(s, 9);
        R(s, 11) += R(s, 6);
        R(s, 10) = RotateMask(R(s, 10), 2u, 0xfffffffcu);
        R(s, 10) = m.ReadU32(W(R(s, 26) + R(s, 10)));
        R(s, 10) = m.ReadU32(W(R(s, 8) + R(s, 10)));
        CmpS(s, R(s, 10), 0);
        if (s.cr6.eq)
        {
            s.f0_bits = LoadF(m, native, s, R(s, 31) + 140u);
            s.f12_bits = LoadF(m, native, s, R(s, 11) + 8u);
            CmpF(s, s.f0_bits, s.f12_bits);
            if (!s.cr6.lt)
            {
                s.f0_bits = LoadF(m, native, s, R(s, 28));
                CmpF(s, s.f0_bits, s.f13_bits);
                if (s.cr6.lt)
                    StoreF(m, native, s, R(s, 28), s.f13_bits);
                R(s, 10) = m.ReadU32(W(R(s, 11)));
                R(s, 9) = m.ReadU32(W(R(s, 11) + 4u));
                CmpS(s, R(s, 9), 0);
                m.WriteU32(W(R(s, 1) + 84u), W(R(s, 10)));
                if (!s.cr6.eq)
                {
                    R(s, 10) = m.ReadU32(W(R(s, 5) + 13948u));
                    R(s, 26) = R(s, 1) + 80u;
                    R(s, 25) = R(s, 1) + 84u;
                    R(s, 10) = std::uint64_t(std::int64_t(S(R(s, 10))) *
                        std::int64_t(S(R(s, 3))));
                    R(s, 10) += R(s, 4);
                    R(s, 9) = (W(R(s, 10)) & 0x7fffffu) | 0x3f800000u;
                    m.WriteU32(W(R(s, 5) + 13948u), W(R(s, 10)));
                    R(s, 10) = m.ReadU32(W(R(s, 11) + 4u));
                    R(s, 11) = m.ReadU32(W(R(s, 11)));
                    m.WriteU32(W(R(s, 1) + 88u), W(R(s, 9)));
                    R(s, 11) -= R(s, 10);
                    R(s, 10) = std::uint64_t(std::int64_t(S(R(s, 10))));
                    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))));
                    WriteU64(m, W(R(s, 1) + 96u), R(s, 10));
                    WriteU64(m, W(R(s, 1) + 104u), R(s, 11));
                    s.f0_bits = LoadF(m, native, s, R(s, 1) + 88u);
                    s.f12_bits = ConvertWord(F(s.f0_bits));
                    m.WriteU32(W(R(s, 26)), W(s.f12_bits));
                    s.f12_bits = ReadU64(m, W(R(s, 1) + 96u));
                    s.f11_bits = ReadU64(m, W(R(s, 1) + 104u));
                    s.f12_bits = Single(double(std::bit_cast<std::int64_t>(s.f12_bits)));
                    s.f11_bits = Single(double(std::bit_cast<std::int64_t>(s.f11_bits)));
                    R(s, 11) = m.ReadU32(W(R(s, 1) + 80u));
                    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))));
                    WriteU64(m, W(R(s, 1) + 112u), R(s, 11));
                    s.f10_bits = ReadU64(m, W(R(s, 1) + 112u));
                    s.f10_bits = Single(double(std::bit_cast<std::int64_t>(s.f10_bits)));
                    s.f0_bits = Single(F(s.f0_bits) - F(s.f10_bits));
                    s.f0_bits = Single(std::fma(F(s.f0_bits),
                        F(s.f11_bits), F(s.f12_bits)));
                    s.f0_bits = ConvertWord(F(s.f0_bits));
                    m.WriteU32(W(R(s, 25)), W(s.f0_bits));
                    R(s, 10) = m.ReadU32(W(R(s, 1) + 84u));
                }
                R(s, 9) = std::uint64_t(std::int64_t(S(R(s, 10))));
                R(s, 11) = m.ReadU32(W(R(s, 30)));
                s.f0_bits = LoadF(m, native, s, R(s, 28));
                R(s, 11) += R(s, 10);
                WriteU64(m, W(R(s, 1) + 120u), R(s, 9));
                m.WriteU32(W(R(s, 30)), W(R(s, 11)));
                R(s, 11) = m.ReadU32(W(R(s, 7) + 60u));
                R(s, 9) = m.ReadU32(W(R(s, 31) + 184u));
                R(s, 10) = RotateMask(R(s, 11), 1u, 0xfffffffeu);
                R(s, 11) += R(s, 10);
                R(s, 11) = RotateMask(R(s, 11), 2u, 0xfffffffcu);
                R(s, 11) = m.ReadU32(W(R(s, 9) + R(s, 11)));
                m.WriteU32(W(R(s, 11) + R(s, 8)), W(R(s, 29)));
                s.f12_bits = ReadU64(m, W(R(s, 1) + 120u));
                s.f12_bits = Single(double(std::bit_cast<std::int64_t>(s.f12_bits)));
                s.f0_bits = Single(F(s.f12_bits) / F(s.f0_bits));
                s.f1_bits = Single(F(s.f0_bits) + F(s.f1_bits));
            }
        }
        R(s, 11) = m.ReadU32(W(R(s, 7) + 72u));
        R(s, 27) += 1u;
        R(s, 6) += 12u;
        R(s, 8) += 4u;
        R(s, 10) = m.ReadU32(W(R(s, 11) + 124u));
        CmpS(s, R(s, 27), R(s, 10));
        if (!s.cr6.lt) break;
    }
    Restore25(m, s);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry == 0x82607318u)
    {
        SelectSample(memory, native, state);
        return true;
    }
    if (entry == 0x822c79b0u)
    {
        Accumulate(memory, native, state);
        return true;
    }
    return false;
}

} // namespace lo::semantic::gpu::object_sample_accumulator
