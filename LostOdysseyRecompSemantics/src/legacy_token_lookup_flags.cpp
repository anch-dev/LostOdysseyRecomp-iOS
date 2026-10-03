#include "lo_semantics/legacy_token_lookup_flags.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_token_lookup_flags
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

void CompareUnsigned(Registers& state, std::uint64_t left,
    std::uint32_t right)
{
    const auto word = static_cast<std::uint32_t>(left);
    state.cr6 = {std::uint8_t(word < right), std::uint8_t(word > right),
        std::uint8_t(word == right), state.xer_so};
}

void CompareSigned(Registers& state, std::uint64_t left,
    std::int32_t right)
{
    const auto word = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    state.cr6 = {std::uint8_t(word < right), std::uint8_t(word > right),
        std::uint8_t(word == right), state.xer_so};
}

void CompareSigned(Registers& state, std::uint64_t left,
    std::uint64_t right)
{
    CompareSigned(state, left, std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(right)));
}

void FindKey(GuestMemory& memory, Registers& state)
{
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 12u));
    CompareUnsigned(state, R(state, 11), 0u);
    if (state.cr6.eq) { R(state, 3) = 0u; return; }
    R(state, 10) = memory.ReadU32(Address(R(state, 3) + 4u));
    CompareSigned(state, R(state, 10), 0);
    if (!state.cr6.gt) { R(state, 3) = 0u; return; }

    R(state, 9) = ReadU64(memory, Address(R(state, 4)));
    R(state, 10) = memory.ReadU32(Address(R(state, 3) + 16u));
    R(state, 10) -= 1u;
    WriteU64(memory, Address(state.sp - 16u), R(state, 9));
    R(state, 9) = memory.ReadU32(Address(state.sp - 16u));
    R(state, 10) &= R(state, 9);
    R(state, 10) = WordRotateMask(R(state, 10), 2, 0xfffffffcu);
    R(state, 10) = memory.ReadU32(Address(R(state, 10) + R(state, 11)));
    CompareSigned(state, R(state, 10), -1);
    if (state.cr6.eq) { R(state, 3) = 0u; return; }
    R(state, 8) = memory.ReadU32(Address(R(state, 4)));
    R(state, 9) = memory.ReadU32(Address(R(state, 3)));

    for (;;)
    {
        R(state, 11) = WordRotateMask(R(state, 10), 4, 0xfffffff0u);
        R(state, 11) += R(state, 9);
        R(state, 7) = memory.ReadU32(Address(R(state, 11) + 4u));
        CompareSigned(state, R(state, 7), R(state, 8));
        if (state.cr6.eq)
        {
            R(state, 7) = memory.ReadU32(Address(R(state, 11) + 8u));
            R(state, 6) = memory.ReadU32(Address(R(state, 4) + 4u));
            CompareSigned(state, R(state, 7), R(state, 6));
            if (state.cr6.eq)
            {
                R(state, 11) = WordRotateMask(R(state, 10), 4,
                    0xfffffff0u);
                R(state, 11) += R(state, 9);
                R(state, 3) = memory.ReadU32(Address(R(state, 11) + 12u));
                return;
            }
        }
        R(state, 10) = memory.ReadU32(Address(R(state, 11)));
        CompareSigned(state, R(state, 10), -1);
        if (state.cr6.eq) { R(state, 3) = 0u; return; }
    }
}

void CharacterFlags(GuestMemory& memory, Registers& state)
{
    R(state, 11) = R(state, 3) & 0xffffu;
    CompareUnsigned(state, R(state, 11), 65535u);
    if (!state.cr6.eq)
    {
        CompareUnsigned(state, R(state, 11), 256u);
        if (state.cr6.lt)
        {
            R(state, 10) = WordRotateMask(R(state, 11), 1,
                0xfffffffeu);
            R(state, 11) = static_cast<std::uint64_t>(
                std::int64_t{-2094989312});
            R(state, 9) = R(state, 4) & 0xffffu;
            R(state, 11) = memory.ReadU32(Address(R(state, 11) + 23360u));
            R(state, 11) = memory.ReadU16(Address(R(state, 10) +
                R(state, 11)));
            R(state, 11) &= R(state, 9);
        }
        else R(state, 11) = 0u;
    }
    else R(state, 11) = 0u;
    R(state, 11) &= 0xffffu;
    R(state, 10) = R(state, 4) & 0xffffu;
    R(state, 3) = R(state, 11) & R(state, 10);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    switch (entry)
    {
    case 0x822972a8u: FindKey(memory, registers); return true;
    case 0x822974b0u: CharacterFlags(memory, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_token_lookup_flags
