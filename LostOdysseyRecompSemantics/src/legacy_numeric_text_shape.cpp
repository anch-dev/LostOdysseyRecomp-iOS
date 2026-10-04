#include "lo_semantics/legacy_numeric_text_shape.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_numeric_text_shape
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{ return index == 1u ? state.sp : state.r[index]; }

void CompareSigned(Registers& state, std::uint64_t left,
    std::int32_t right)
{
    const auto word = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    state.cr6 = {std::uint8_t(word < right), std::uint8_t(word > right),
        std::uint8_t(word == right), state.xer_so};
}

void CompareUnsigned(Registers& state, std::uint64_t left,
    std::uint32_t right)
{
    const auto word = static_cast<std::uint32_t>(left);
    state.cr6 = {std::uint8_t(word < right), std::uint8_t(word > right),
        std::uint8_t(word == right), state.xer_so};
}

void LengthMinusOne(GuestMemory& memory, Registers& state)
{
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 4u));
    CompareSigned(state, R(state, 11), 0);
    R(state, 3) = R(state, 11) + UINT64_MAX;
    if (state.cr6.eq) R(state, 3) = 0u;
}

void Save29(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 29u; index <= 31u; ++index)
        WriteU64(memory, Address(state.sp - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
}

void Restore29(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 29u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void ValidateText(GuestMemory& memory, Registers& state)
{
    Save29(memory, state);
    state.lr = 0x82479190u;
    memory.WriteU32(Address(state.sp - 112u), Address(state.sp));
    state.sp -= 112u;

    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 4u));
    CompareSigned(state, R(state, 11), 0);
    if (state.cr6.eq) goto invalid;
    R(state, 11) += UINT64_MAX;
    CompareSigned(state, R(state, 11), 0);
    if (state.cr6.eq) goto invalid;

    R(state, 30) = memory.ReadU32(Address(R(state, 3)));
    R(state, 11) = memory.ReadU16(Address(R(state, 30)));
    CompareUnsigned(state, R(state, 11), 45u);
    if (state.cr6.eq) goto valid_first;
    CompareUnsigned(state, R(state, 11), 46u);
    if (state.cr6.eq) goto valid_first;
    CompareUnsigned(state, R(state, 11), 48u);
    if (state.cr6.lt) goto invalid;
    CompareUnsigned(state, R(state, 11), 57u);
    if (state.cr6.gt) goto invalid;

valid_first:
    R(state, 11) += static_cast<std::uint64_t>(-46);
    R(state, 31) = 1u;
    R(state, 11) = std::countl_zero(static_cast<std::uint32_t>(R(state, 11)));
    R(state, 29) = std::rotl(static_cast<std::uint32_t>(R(state, 11)),
        27) & 1u;
    state.lr = 0x824791f4u;
    LengthMinusOne(memory, state);
    CompareSigned(state, R(state, 3), 1);
    if (!state.cr6.gt) goto valid;
    R(state, 10) = R(state, 30) + 2u;

scan:
    R(state, 11) = memory.ReadU16(Address(R(state, 10)));
    CompareUnsigned(state, R(state, 11), 46u);
    if (!state.cr6.eq) goto digit;
    CompareSigned(state, R(state, 29), 0);
    if (!state.cr6.eq) goto invalid;
    R(state, 29) = 1u;
    goto advance;

digit:
    CompareUnsigned(state, R(state, 11), 48u);
    if (state.cr6.lt) goto invalid;
    CompareUnsigned(state, R(state, 11), 57u);
    if (state.cr6.gt) goto invalid;

advance:
    R(state, 31) += 1u;
    R(state, 10) += 2u;
    CompareSigned(state, R(state, 31),
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(R(state, 3))));
    if (state.cr6.lt) goto scan;

valid:
    R(state, 3) = 1u;
    goto done;

invalid:
    R(state, 3) = 0u;

done:
    state.sp += 112u;
    Restore29(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    switch (entry)
    {
    case 0x823f7bf8u: LengthMinusOne(memory, registers); return true;
    case 0x82479188u: ValidateText(memory, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_numeric_text_shape
