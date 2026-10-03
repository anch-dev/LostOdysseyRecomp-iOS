#include "lo_semantics/crt_exception_predicates.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::crt_exception_predicates
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

void FrameByteIsSet(GuestMemory& memory, Registers& state)
{
    WriteU64(memory, Address(state.sp - 8u), state.r31);
    state.r31 = state.r12 - 96u;
    state.r11 = memory.ReadU8(Address(state.r31 + 127u));
    state.r11 = std::countl_zero(static_cast<std::uint32_t>(state.r11));
    state.r11 = WordRotateMask(state.r11, 27, 1u);
    state.r3 = state.r11 ^ 1u;
    state.r31 = ReadU64(memory, Address(state.sp - 8u));
}

void IsNoMemoryException(GuestMemory& memory, Registers& state)
{
    state.r11 = memory.ReadU32(Address(state.r3));
    state.r10 = 0xffffffffc0000000ull;
    state.r10 |= 23u;
    state.r11 = memory.ReadU32(Address(state.r11));
    state.r11 = state.r10 - state.r11;
    state.r11 = std::countl_zero(static_cast<std::uint32_t>(state.r11));
    state.r3 = WordRotateMask(state.r11, 27, 1u);
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    switch (entry)
    {
    case 0x82b80480u: FrameByteIsSet(memory, state); return true;
    case 0x82b8223cu: IsNoMemoryException(memory, state); return true;
    default: return false;
    }
}
}
