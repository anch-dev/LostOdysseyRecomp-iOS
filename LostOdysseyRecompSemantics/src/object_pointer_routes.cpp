#include "lo_semantics/object_pointer_routes.h"
#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::object_pointer_routes
{
namespace
{
using recovery_abi::Address;

void CompareUnsigned(Registers& state, std::uint32_t left,
    std::uint32_t right)
{
    state.cr6 = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), state.xer_so};
}

void CompareSigned(Registers& state, std::int32_t left, std::int32_t right)
{
    state.cr6 = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), state.xer_so};
}

void FindTaggedObject(GuestMemory& memory, Registers& state)
{
    CompareUnsigned(state, Address(state.r4), 0u);
    if (state.cr6.eq != 0u)
    {
        state.r3 = 0u;
        return;
    }
    state.r9 = memory.ReadU32(Address(state.r3 + 288u));
    state.r10 = 0u;
    CompareSigned(state, static_cast<std::int32_t>(state.r9), 0);
    if (state.cr6.gt == 0u)
    {
        state.r3 = 0u;
        return;
    }
    state.r8 = memory.ReadU32(Address(state.r3 + 284u));
    state.r11 = state.r8;
    while (true)
    {
        state.r7 = memory.ReadU32(Address(state.r11));
        state.r7 = memory.ReadU32(Address(state.r7 + 60u));
        CompareUnsigned(state, Address(state.r7), Address(state.r4));
        if (state.cr6.eq != 0u)
        {
            state.r11 = (static_cast<std::uint32_t>(state.r10) << 2u) &
                0xfffffffcu;
            state.r3 = memory.ReadU32(Address(state.r11 + state.r8));
            return;
        }
        state.r10 += 1u;
        state.r11 += 4u;
        CompareSigned(state, static_cast<std::int32_t>(state.r10),
            static_cast<std::int32_t>(state.r9));
        if (state.cr6.lt == 0u)
            break;
    }
    state.r3 = 0u;
}

void PreferredObject(GuestMemory& memory, Registers& state)
{
    state.r10 = memory.ReadU32(Address(state.r3 + 624u));
    CompareUnsigned(state, Address(state.r10), 0u);
    if (state.cr6.eq != 0u)
    {
        state.r3 = 0u;
        return;
    }
    state.r11 = memory.ReadU32(Address(state.r3 + 888u));
    CompareUnsigned(state, Address(state.r11), 0u);
    if (state.cr6.eq != 0u)
        state.r11 = memory.ReadU32(Address(state.r10 + 244u));
    state.r3 = state.r11;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    switch (entry)
    {
    case 0x822c6398u: FindTaggedObject(memory, state); return true;
    case 0x822c66c8u: PreferredObject(memory, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::object_pointer_routes
