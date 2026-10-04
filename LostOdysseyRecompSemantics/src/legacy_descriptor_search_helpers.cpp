#include "lo_semantics/legacy_descriptor_search_helpers.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_search_helpers
{
namespace
{
using recovery_abi::Address;
using recovery_abi::WordRotateMask;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& s, unsigned index)
{ return index == 1u ? s.sp : s.r[index]; }
void Load(GuestMemory& m, Registers& s, unsigned index,
    std::uint64_t address)
{ R(s, index) = m.ReadU32(Address(address)); }
void Compare(Registers& s, std::uint64_t a, std::uint64_t b,
    Condition& cr)
{
    const auto x = Address(a), y = Address(b);
    cr = {std::uint8_t(x < y), std::uint8_t(x > y),
        std::uint8_t(x == y), s.xer_so};
}
void CompareS(Registers& s, std::uint64_t a, Condition& cr)
{
    const auto x = std::bit_cast<std::int32_t>(Address(a));
    cr = {std::uint8_t(x < 0), std::uint8_t(x > 0),
        std::uint8_t(x == 0), s.xer_so};
}
std::uint64_t Left(std::uint64_t value, std::uint64_t shift)
{
    const auto n = static_cast<std::uint8_t>(shift);
    return n & 32u ? 0u : Address(Address(value) << (n & 63u));
}
std::uint64_t Right(std::uint64_t value, std::uint64_t shift)
{
    const auto n = static_cast<std::uint8_t>(shift);
    return n & 32u ? 0u : Address(value) >> (n & 63u);
}
void IsOrdinalType(GuestMemory& m, Registers& s)
{
    Load(m, s, 11u, R(s, 3) + 8u);
    R(s, 11) = WordRotateMask(R(s, 11), 25, 0x7fu);
    Compare(s, R(s, 11), 21u, s.cr6);
    if (s.cr6.lt) R(s, 10) = 0u;
    else
    {
        Compare(s, R(s, 11), 24u, s.cr6);
        R(s, 10) = s.cr6.gt ? 0u : 1u;
    }
    R(s, 10) = Address(R(s, 10)) & 0xffu;
    CompareS(s, R(s, 10), s.cr0);
    if (s.cr0.eq)
    {
        Compare(s, R(s, 11), 62u, s.cr6);
        if (!s.cr6.lt)
        {
            Compare(s, R(s, 11), 65u, s.cr6);
            if (!s.cr6.gt) { R(s, 11) = 1u; R(s, 3) = 1u; return; }
        }
        Compare(s, R(s, 11), 108u, s.cr6);
        if (s.cr6.eq) { R(s, 11) = 1u; R(s, 3) = 1u; return; }
        Compare(s, R(s, 11), 107u, s.cr6);
        R(s, 11) = s.cr6.eq ? 1u : 0u;
    }
    else R(s, 11) = 1u;
    R(s, 3) = Address(R(s, 11)) & 0xffu;
}
void HasDescriptorFeature(GuestMemory& m, Registers& s)
{
    Load(m, s, 11u, R(s, 3));
    R(s, 11) = WordRotateMask(R(s, 11), 0, 0x0e000000u);
    CompareS(s, R(s, 11), s.cr0);
    if (s.cr0.eq) { R(s, 11) = 0u; R(s, 3) = 0u; return; }
    Load(m, s, 10u, R(s, 3) + 12u);
    Load(m, s, 11u, R(s, 10) + 8u);
    R(s, 11) = WordRotateMask(R(s, 11), 25, 0x7fu);
    Compare(s, R(s, 11), 124u, s.cr6);
    if (!s.cr6.eq)
    {
        Compare(s, R(s, 11), 123u, s.cr6);
        R(s, 11) = s.cr6.eq ? 1u : 0u;
    }
    else R(s, 11) = 1u;
    R(s, 11) = Address(R(s, 11)) & 0xffu;
    CompareS(s, R(s, 11), s.cr0);
    if (!s.cr0.eq) { R(s, 11) = 0u; R(s, 3) = 0u; return; }
    Load(m, s, 11u, R(s, 10) + 8u);
    R(s, 11) = WordRotateMask(R(s, 11), 27, 1u);
    CompareS(s, R(s, 11), s.cr0);
    R(s, 11) = s.cr0.eq ? 1u : 0u;
    R(s, 3) = Address(R(s, 11)) & 0xffu;
}
void NormalizePairs(Registers& s)
{
    R(s, 9) = 0u; R(s, 10) = 0u; R(s, 11) = 0u;
    do
    {
        R(s, 8) = Right(R(s, 3), R(s, 11));
        R(s, 7) = 3u;
        R(s, 8) += R(s, 10);
        R(s, 10) += 1u;
        R(s, 8) = Address(R(s, 8)) & 3u;
        R(s, 7) = Left(R(s, 7), R(s, 11));
        R(s, 9) &= ~R(s, 7);
        R(s, 8) = Left(R(s, 8), R(s, 11));
        R(s, 11) += 2u;
        R(s, 9) |= R(s, 8);
        Compare(s, R(s, 11), 8u, s.cr6);
    } while (s.cr6.lt);
    R(s, 3) = R(s, 9);
}
void WalkLink(GuestMemory& m, Registers& s)
{
    R(s, 11) = R(s, 3);
    for (;;)
    {
        R(s, 11) = WordRotateMask(R(s, 11), 0, 0xfffffffeu);
        Load(m, s, 11u, R(s, 11) + 32u);
        R(s, 10) = Address(R(s, 11)) & 1u;
        CompareS(s, R(s, 10), s.cr0);
        if (!s.cr0.eq) { R(s, 3) = 0u; return; }
        R(s, 11) = WordRotateMask(R(s, 11), 0, 0xfffffffeu);
        s.xer_ca = std::uint8_t(Address(R(s, 11)) > 35u);
        R(s, 11) -= 36u;
        CompareS(s, R(s, 11), s.cr0);
        if (s.cr0.eq) { R(s, 3) = 0u; return; }
        Compare(s, R(s, 11), R(s, 4), s.cr6);
        if (s.cr6.eq) { R(s, 3) = 1u; return; }
    }
}
void Overlaps(GuestMemory& m, Registers& s)
{
    R(s, 11) = R(s, 3);
    R(s, 3) = R(s, 4);
    R(s, 10) = m.ReadU8(Address(R(s, 11) + 12u));
    R(s, 10) = Address(R(s, 10)) & 1u;
    CompareS(s, R(s, 10), s.cr0);
    if (s.cr0.eq) { R(s, 4) = R(s, 11); WalkLink(m, s); return; }
    R(s, 10) = m.ReadU8(Address(R(s, 3) + 12u));
    R(s, 10) = Address(R(s, 10)) & 1u;
    CompareS(s, R(s, 10), s.cr0);
    if (s.cr0.eq) { R(s, 4) = R(s, 11); WalkLink(m, s); return; }
    Load(m, s, 10u, R(s, 11) + 16u);
    Load(m, s, 9u, R(s, 3) + 16u);
    R(s, 10) = Address(R(s, 10)) & 0x1fffu;
    R(s, 9) = Address(R(s, 9)) & 0x1fffu;
    Compare(s, R(s, 10), R(s, 9), s.cr6);
    if (s.cr6.gt) { R(s, 3) = 1u; return; }
    for (;;)
    {
        R(s, 11) = WordRotateMask(R(s, 11), 0, 0xfffffffeu);
        Load(m, s, 11u, R(s, 11) + 36u);
        R(s, 9) = Address(R(s, 11)) & 1u;
        CompareS(s, R(s, 9), s.cr0);
        if (!s.cr0.eq) { R(s, 3) = 0u; return; }
        Compare(s, R(s, 11), 0u, s.cr6);
        if (s.cr6.eq) { R(s, 3) = 0u; return; }
        Load(m, s, 9u, R(s, 11) + 16u);
        R(s, 9) = Address(R(s, 9)) & 0x1fffu;
        Compare(s, R(s, 9), R(s, 10), s.cr6);
        if (s.cr6.lt) { R(s, 3) = 0u; return; }
        Compare(s, R(s, 11), R(s, 3), s.cr6);
        if (s.cr6.eq) { R(s, 3) = 1u; return; }
    }
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    switch (entry)
    {
    case 0x82fac070u: IsOrdinalType(memory, registers); return true;
    case 0x82fac608u: HasDescriptorFeature(memory, registers); return true;
    case 0x83054600u: NormalizePairs(registers); return true;
    case 0x82fb71d0u: Overlaps(memory, registers); return true;
    case 0x83057b58u: WalkLink(memory, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_search_helpers
