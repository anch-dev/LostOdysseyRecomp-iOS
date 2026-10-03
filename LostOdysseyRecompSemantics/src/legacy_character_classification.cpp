#include "lo_semantics/legacy_character_classification.h"
#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::legacy_character_classification
{
namespace
{
using recovery_abi::Address;

void Compare(Registers& state, std::uint32_t value, std::uint32_t bound)
{
    state.cr6 = {std::uint8_t(value < bound), std::uint8_t(value > bound),
        std::uint8_t(value == bound), state.xer_so};
}
bool InRange(Registers& state, std::uint32_t value,
    std::uint32_t first, std::uint32_t last)
{
    Compare(state, value, first);
    if (state.cr6.lt) return false;
    Compare(state, value, last);
    return !state.cr6.gt;
}
bool Equals(Registers& state, std::uint32_t value, std::uint32_t bound)
{
    Compare(state, value, bound);
    return state.cr6.eq != 0;
}
void IsLegacyLetter(Registers& state)
{
    state.r11 = state.r3 & 0xffffu;
    const auto value = static_cast<std::uint32_t>(state.r11);
    state.r3 = InRange(state, value, 65, 90) ||
        InRange(state, value, 192, 255) || InRange(state, value, 97, 122) ||
        Equals(state, value, 159) || Equals(state, value, 140) ||
        Equals(state, value, 156);
}
void IsLegacyAlphanumeric(GuestMemory& memory, Registers& state)
{
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    recovery_abi::WriteU64(memory, Address(state.sp - 16u), state.r31);
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    state.r31 = state.r3;
    state.lr = 0x82296950u;
    IsLegacyLetter(state);
    Compare(state, static_cast<std::uint32_t>(state.r3), 0);
    bool accepted = !state.cr6.eq;
    if (!accepted)
    {
        state.r11 = state.r31 & 0xffffu;
        accepted = InRange(state, static_cast<std::uint32_t>(state.r11), 48, 57);
    }
    state.r3 = accepted;
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    state.r31 = recovery_abi::ReadU64(memory, Address(state.sp - 16u));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    switch (entry)
    {
    case 0x82483ac8u: IsLegacyLetter(state); return true;
    case 0x82296938u: IsLegacyAlphanumeric(memory, state); return true;
    default: return false;
    }
}
}
