#include "lo_semantics/legacy_character_cursor.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_character_cursor
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{
    return state.r[index];
}

void CompareUnsigned(Registers& state, std::uint64_t lhs,
    std::uint32_t rhs)
{
    const auto word = static_cast<std::uint32_t>(lhs);
    state.cr6 = {std::uint8_t(word < rhs), std::uint8_t(word > rhs),
        std::uint8_t(word == rhs), state.xer_so};
}

void CompareSigned(Registers& state, std::uint64_t lhs,
    std::uint64_t rhs)
{
    const auto a = static_cast<std::int32_t>(lhs);
    const auto b = static_cast<std::int32_t>(rhs);
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}

void SkipWhitespace(GuestMemory& memory, Registers& state)
{
    for (;;)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 3)));
        R(state, 10) = memory.ReadU16(Address(R(state, 11)));
        CompareUnsigned(state, R(state, 10), 32u);
        if (!state.cr6.eq)
        {
            CompareUnsigned(state, R(state, 10), 9u);
            if (!state.cr6.eq) return;
        }
        R(state, 11) += 2u;
        memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
    }
}

void FinishQuotedToken(GuestMemory& memory, Registers& state)
{
    R(state, 11) = memory.ReadU32(Address(R(state, 3)));
    R(state, 10) = memory.ReadU16(Address(R(state, 11)));
    CompareUnsigned(state, R(state, 10), 34u);
    if (state.cr6.eq)
    {
        R(state, 11) += 2u;
        memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
    }
}

void ReadQuotedToken(GuestMemory& memory, Registers& state)
{
    R(state, 10) = R(state, 11) + 2u;
    memory.WriteU32(Address(R(state, 3)), Address(R(state, 10)));
    R(state, 11) = memory.ReadU16(Address(R(state, 11) + 2u));
    CompareUnsigned(state, R(state, 11), 0u);
    if (!state.cr6.eq)
    {
        R(state, 8) = 1u;
        R(state, 9) = R(state, 4);
        for (;;)
        {
            R(state, 10) = memory.ReadU32(Address(R(state, 3)));
            R(state, 11) = memory.ReadU16(Address(R(state, 10)));
            CompareUnsigned(state, R(state, 11), 34u);
            if (state.cr6.eq) break;
            CompareSigned(state, R(state, 8), R(state, 5));
            if (!state.cr6.lt) break;

            R(state, 10) += 2u;
            R(state, 31) = R(state, 11) & 0xffffu;
            CompareUnsigned(state, R(state, 31), 92u);
            memory.WriteU32(Address(R(state, 3)), Address(R(state, 10)));
            if (state.cr6.eq)
            {
                CompareSigned(state, R(state, 6), 0u);
                if (!state.cr6.eq)
                {
                    R(state, 11) = memory.ReadU16(Address(R(state, 10)));
                    R(state, 10) += 2u;
                    CompareUnsigned(state, R(state, 11), 0u);
                    memory.WriteU32(Address(R(state, 3)), Address(R(state, 10)));
                    if (state.cr6.eq) break;
                }
            }

            memory.WriteU16(Address(R(state, 9)),
                static_cast<std::uint16_t>(R(state, 11)));
            ++R(state, 7);
            R(state, 11) = memory.ReadU32(Address(R(state, 3)));
            ++R(state, 8);
            R(state, 9) += 2u;
            R(state, 11) = memory.ReadU16(Address(R(state, 11)));
            CompareUnsigned(state, R(state, 11), 0u);
            if (state.cr6.eq) break;
        }
    }
    FinishQuotedToken(memory, state);
}

void ReadUnquotedToken(GuestMemory& memory, Registers& state)
{
    CompareUnsigned(state, R(state, 10), 0u);
    if (state.cr6.eq) return;

    R(state, 9) = 1u;
    R(state, 8) = R(state, 4);
    for (;;)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 3)));
        R(state, 11) = memory.ReadU16(Address(R(state, 11)));
        R(state, 10) = R(state, 11);
        CompareUnsigned(state, R(state, 10), 32u);
        if (state.cr6.eq) break;
        CompareUnsigned(state, R(state, 10), 9u);
        if (state.cr6.eq) break;
        CompareSigned(state, R(state, 9), R(state, 5));
        if (state.cr6.lt)
        {
            memory.WriteU16(Address(R(state, 8)),
                static_cast<std::uint16_t>(R(state, 11)));
            ++R(state, 7);
            ++R(state, 9);
            R(state, 8) += 2u;
        }
        R(state, 11) = memory.ReadU32(Address(R(state, 3)));
        R(state, 11) += 2u;
        memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
        R(state, 11) = WordRotateMask(R(state, 11), 0, 0xffffffffu);
        R(state, 11) = memory.ReadU16(Address(R(state, 11)));
        CompareUnsigned(state, R(state, 11), 0u);
        if (state.cr6.eq) break;
    }
}

void ReadToken(GuestMemory& memory, Registers& state)
{
    WriteU64(memory, Address(state.sp - 8u), R(state, 31));
    R(state, 7) = 0u;
    SkipWhitespace(memory, state);
    R(state, 11) = memory.ReadU32(Address(R(state, 3)));
    R(state, 10) = memory.ReadU16(Address(R(state, 11)));
    CompareUnsigned(state, R(state, 10), 34u);
    if (state.cr6.eq)
        ReadQuotedToken(memory, state);
    else
        ReadUnquotedToken(memory, state);

    R(state, 11) = WordRotateMask(R(state, 7), 1, 0xfffffffeu);
    R(state, 10) = static_cast<std::uint32_t>(R(state, 7)) == 0u ?
        32u : std::countl_zero(static_cast<std::uint32_t>(R(state, 7)));
    R(state, 9) = 0u;
    R(state, 10) = WordRotateMask(R(state, 10), 27, 1u);
    R(state, 3) = R(state, 10) ^ 1u;
    memory.WriteU16(Address(R(state, 11) + R(state, 4)), 0u);
    R(state, 31) = ReadU64(memory, Address(state.sp - 8u));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    if (entry != 0x822969a0u) return false;
    ReadToken(memory, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_character_cursor
