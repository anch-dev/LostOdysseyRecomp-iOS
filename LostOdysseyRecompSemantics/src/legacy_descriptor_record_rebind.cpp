#include "lo_semantics/legacy_descriptor_record_rebind.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::legacy_descriptor_record_rebind
{
namespace
{
using recovery_abi::Address;
using recovery_abi::WordRotateMask;
void Compare(Registers& s, std::uint64_t a, std::uint64_t b,
    crt_stream_operations::Condition& c, bool signed_words = false)
{
    const auto x = Address(a), y = Address(b);
    const auto sx = std::bit_cast<std::int32_t>(x), sy = std::bit_cast<std::int32_t>(y);
    c = {std::uint8_t(signed_words ? sx < sy : x < y),
        std::uint8_t(signed_words ? sx > sy : x > y), std::uint8_t(x == y), s.xer_so};
}
void Rebind(GuestMemory& m, Registers& s)
{
    auto& r = s.r;
    r[11] = m.ReadU32(Address(r[4]));
    r[10] = WordRotateMask(r[11], 0, 0x40000000u);
    Compare(s, r[10], 0u, s.cr0, true);
    if (!s.cr0.eq)
    {
        r[11] = WordRotateMask(r[11], 0, 0x1fff8u);
        Compare(s, r[11], 0u, s.cr0, true); r[7] = 0u;
        if (s.cr0.eq) return;
        r[10] = 0u;
        do
        {
            r[11] = m.ReadU32(Address(r[4] + 28u));
            r[11] = m.ReadU32(Address(r[10] + r[11]));
            r[9] = WordRotateMask(r[11], 28, 0x3fffu);
            Compare(s, r[9], r[5], s.cr6);
            if (s.cr6.eq)
            {
                r[9] = Address(r[11]) & 15u;
                r[11] = m.ReadU32(Address(r[4] + 24u)); r[11] += r[10];
                r[9] = r[9] == 0u ? 32u : std::countl_zero(Address(r[9]));
                s.xer_ca = std::uint8_t(Address(r[9]) <= 31u); r[9] = 31u - r[9];
                r[6] = m.ReadU32(Address(r[11] + 4u));
                m.WriteU32(Address(r[11]), Address(r[8]));
                r[6] = WordRotateMask(r[9], 2, 0xfffcu) | (r[6] & 0xffffffffffff0003ull);
                r[9] = 1u;
                r[6] = WordRotateMask(r[9], 0, 0xfffffffffffe0003ull) | (r[6] & 0x1fffcu);
                m.WriteU32(Address(r[11] + 4u), Address(r[6]));
            }
            r[11] = m.ReadU32(Address(r[4])); ++r[7]; r[10] += 8u;
            r[11] = WordRotateMask(r[11], 29, 0x3fffu);
            Compare(s, r[7], r[11], s.cr6);
        } while (s.cr6.lt);
        return;
    }
    Compare(s, r[6], r[7], s.cr6);
    if (!s.cr6.lt) return;
    r[5] = WordRotateMask(r[6], 3, 0xfffffff8u); r[9] = 0u; r[10] = r[7] - r[6];
    do
    {
        r[11] = m.ReadU32(Address(r[4] + 24u)); r[7] = r[9];
        s.xer_ca = std::uint8_t(Address(r[10]) > 0u); --r[10];
        Compare(s, r[10], 0u, s.cr0, true); r[11] += r[5]; r[9] += 4u; r[5] += 8u;
        r[6] = m.ReadU32(Address(r[11] + 4u));
        m.WriteU32(Address(r[11]), Address(r[8]));
        r[6] = WordRotateMask(r[7], 0, 0xfffcu) | (r[6] & 0xffffffffffff0003ull);
        r[7] = 1u;
        r[6] = WordRotateMask(r[7], 0, 0xfffffffffffe0003ull) | (r[6] & 0x1fffcu);
        m.WriteU32(Address(r[11] + 4u), Address(r[6]));
    } while (!s.cr0.eq);
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    if (entry != 0x83053ae8u) return false;
    Rebind(memory, state); return true;
}
}
