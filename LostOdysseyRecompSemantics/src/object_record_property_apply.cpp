#include "lo_semantics/object_record_property_apply.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_record_property_apply
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

void FindRecord(GuestMemory& m, NativeServices& native,
    Registers& s, std::uint32_t type)
{
    WriteU64(m, W(R(s, 1) + 24u), R(s, 4));
    R(s, 8) = m.ReadU32(W(R(s, 1) + 24u));
    R(s, 7) = m.ReadU32(W(R(s, 1) + 28u));
    CmpS(s, R(s, 8), 0);
    if (s.cr6.eq)
    {
        CmpS(s, R(s, 7), 0);
        if (s.cr6.eq)
        {
            R(s, 3) = 0;
            return;
        }
    }
    R(s, 9) = m.ReadU32(W(R(s, 3) + 676u));
    R(s, 10) = 0;
    CmpS(s, R(s, 9), 0);
    if (!s.cr6.gt)
    {
        R(s, 3) = 0;
        return;
    }
    R(s, 11) = m.ReadU32(W(R(s, 3) + 672u));
    for (;;)
    {
        R(s, 6) = m.ReadU32(W(R(s, 11)));
        CmpS(s, R(s, 6), R(s, 8));
        if (s.cr6.eq)
        {
            R(s, 6) = m.ReadU32(W(R(s, 11) + 4u));
            CmpS(s, R(s, 6), R(s, 7));
            if (s.cr6.eq)
            {
                R(s, 6) = m.ReadU8(W(R(s, 11) + 8u));
                CmpU(s, R(s, 6), type);
                if (s.cr6.eq)
                {
                    if (type == 1u)
                    {
                        s.f0_bits = LoadF(m, native, s, R(s, 11) + 12u);
                        R(s, 3) = 1;
                        StoreF(m, R(s, 5), s.f0_bits);
                    }
                    else
                    {
                        R(s, 11) += 16u;
                        R(s, 3) = 1;
                        R(s, 10) = m.ReadU32(W(R(s, 11)));
                        m.WriteU32(W(R(s, 5)), W(R(s, 10)));
                        R(s, 10) = m.ReadU32(W(R(s, 11) + 4u));
                        m.WriteU32(W(R(s, 5) + 4u), W(R(s, 10)));
                        R(s, 11) = m.ReadU32(W(R(s, 11) + 8u));
                        m.WriteU32(W(R(s, 5) + 8u), W(R(s, 11)));
                    }
                    return;
                }
            }
        }
        R(s, 10) += 1u;
        R(s, 11) += 44u;
        CmpS(s, R(s, 10), R(s, 9));
        if (!s.cr6.lt) break;
    }
    R(s, 3) = 0;
}

void Save28(GuestMemory& m, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x822c8438u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 28u; i <= 31u; ++i)
        WriteU64(m, sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(sp - 8u, W(R(s, 12)));
    R(s, 1) -= 144u;
    m.WriteU32(W(R(s, 1)), sp);
}
void Restore28(GuestMemory& m, Registers& s)
{
    R(s, 1) += 144u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 28u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
    s.lr = R(s, 12);
}

void ApplyProperties(GuestMemory& m, NativeServices& native, Registers& s)
{
    Save28(m, s);
    R(s, 30) = R(s, 3);
    R(s, 11) = 0xffffffff832c0000ull;
    R(s, 5) = R(s, 1) + 88u;
    R(s, 31) = m.ReadU32(W(R(s, 30) + 8u));
    R(s, 4) = ReadU64(m, W(R(s, 11) + 6248u));
    R(s, 3) = R(s, 31);
    s.lr = 0x822c8458u;
    FindRecord(m, native, s, 2u);
    R(s, 11) = 0xffffffff832c0000ull;
    R(s, 29) = R(s, 3);
    R(s, 5) = R(s, 1) + 80u;
    R(s, 3) = R(s, 31);
    R(s, 4) = ReadU64(m, W(R(s, 11) + 6256u));
    s.lr = 0x822c8470u;
    FindRecord(m, native, s, 1u);
    R(s, 11) = 0xffffffff832c0000ull;
    R(s, 28) = R(s, 3);
    R(s, 5) = R(s, 1) + 84u;
    R(s, 3) = R(s, 31);
    R(s, 4) = ReadU64(m, W(R(s, 11) + 6264u));
    s.lr = 0x822c8488u;
    FindRecord(m, native, s, 1u);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 732u));
    R(s, 11) = W(R(s, 11)) & 0x200000u;
    CmpU(s, R(s, 11), 0);
    if (!s.cr6.eq)
    {
        s.f0_bits = LoadF(m, native, s, R(s, 31) + 892u);
        CmpS(s, R(s, 28), 0);
        if (!s.cr6.eq)
        {
            s.f13_bits = LoadF(m, native, s, R(s, 1) + 80u);
            s.f0_bits = Single(F(s.f0_bits) * F(s.f13_bits));
        }
        R(s, 28) = 1;
    }
    else s.f0_bits = LoadF(m, native, s, R(s, 1) + 80u);
    CmpS(s, R(s, 29), 0);
    if (s.cr6.eq)
    {
        CmpS(s, R(s, 28), 0);
        if (s.cr6.eq)
        {
            CmpS(s, R(s, 3), 0);
            if (s.cr6.eq)
            {
                Restore28(m, s);
                return;
            }
        }
    }
    R(s, 11) = m.ReadU32(W(R(s, 30) + 124u));
    R(s, 8) = 0;
    CmpS(s, R(s, 11), 0);
    if (!s.cr6.gt)
    {
        Restore28(m, s);
        return;
    }
    s.f13_bits = LoadF(m, native, s, R(s, 1) + 96u);
    R(s, 9) = 0;
    s.f12_bits = LoadF(m, native, s, R(s, 1) + 92u);
    s.f11_bits = LoadF(m, native, s, R(s, 1) + 88u);
    s.f10_bits = LoadF(m, native, s, R(s, 1) + 84u);
    for (;;)
    {
        R(s, 11) = m.ReadU32(W(R(s, 30) + 60u));
        CmpS(s, R(s, 29), 0);
        R(s, 7) = m.ReadU32(W(R(s, 30) + 120u));
        R(s, 10) = m.ReadU32(W(R(s, 30) + 56u));
        R(s, 11) = m.ReadU16(W(R(s, 11) + R(s, 9)));
        R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
            std::int64_t(S(R(s, 7))));
        R(s, 11) += R(s, 10);
        if (!s.cr6.eq)
        {
            s.f9_bits = LoadF(m, native, s, R(s, 11) + 96u);
            s.f8_bits = LoadF(m, native, s, R(s, 11) + 100u);
            s.f7_bits = LoadF(m, native, s, R(s, 11) + 104u);
            s.f9_bits = Single(F(s.f9_bits) * F(s.f11_bits));
            s.f8_bits = Single(F(s.f8_bits) * F(s.f12_bits));
            s.f7_bits = Single(F(s.f7_bits) * F(s.f13_bits));
            StoreF(m, R(s, 11) + 96u, s.f9_bits);
            StoreF(m, R(s, 11) + 100u, s.f8_bits);
            StoreF(m, R(s, 11) + 104u, s.f7_bits);
        }
        CmpS(s, R(s, 28), 0);
        if (!s.cr6.eq)
        {
            s.f9_bits = LoadF(m, native, s, R(s, 11) + 108u);
            s.f9_bits = Single(F(s.f9_bits) * F(s.f0_bits));
            StoreF(m, R(s, 11) + 108u, s.f9_bits);
        }
        CmpS(s, R(s, 3), 0);
        if (!s.cr6.eq)
        {
            s.f9_bits = LoadF(m, native, s, R(s, 11) + 32u);
            R(s, 10) = R(s, 11) + 48u;
            s.f9_bits = Single(F(s.f9_bits) * F(s.f10_bits));
            StoreF(m, R(s, 1) + 88u, s.f9_bits);
            s.f8_bits = LoadF(m, native, s, R(s, 11) + 36u);
            s.f9_bits = LoadF(m, native, s, R(s, 11) + 40u);
            s.f8_bits = Single(F(s.f8_bits) * F(s.f10_bits));
            s.f9_bits = Single(F(s.f9_bits) * F(s.f10_bits));
            StoreF(m, R(s, 1) + 92u, s.f8_bits);
            StoreF(m, R(s, 1) + 96u, s.f9_bits);
            R(s, 11) = m.ReadU32(W(R(s, 1) + 88u));
            R(s, 7) = m.ReadU32(W(R(s, 1) + 92u));
            R(s, 6) = m.ReadU32(W(R(s, 1) + 96u));
            m.WriteU32(W(R(s, 10)), W(R(s, 11)));
            m.WriteU32(W(R(s, 10) + 4u), W(R(s, 7)));
            m.WriteU32(W(R(s, 10) + 8u), W(R(s, 6)));
        }
        R(s, 11) = m.ReadU32(W(R(s, 30) + 124u));
        R(s, 8) += 1u;
        R(s, 9) += 2u;
        CmpS(s, R(s, 8), R(s, 11));
        if (!s.cr6.lt) break;
    }
    Restore28(m, s);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry == 0x822c85b0u)
    {
        FindRecord(memory, native, state, 2u);
        return true;
    }
    if (entry == 0x822c7b88u)
    {
        FindRecord(memory, native, state, 1u);
        return true;
    }
    if (entry == 0x822c8430u)
    {
        ApplyProperties(memory, native, state);
        return true;
    }
    return false;
}
} // namespace lo::semantic::gpu::object_record_property_apply
