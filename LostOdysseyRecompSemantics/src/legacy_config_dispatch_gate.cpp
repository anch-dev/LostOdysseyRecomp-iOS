#include "lo_semantics/legacy_config_dispatch_gate.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_config_dispatch_gate
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;

void CompareSigned(Registers& state, std::uint64_t left)
{
    const auto word = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    state.cr6 = {std::uint8_t(word < 0), std::uint8_t(word > 0),
        std::uint8_t(word == 0), state.xer_so};
}

void CompareUnsigned64(Registers& state, std::uint64_t left)
{
    state.cr6 = {0u, std::uint8_t(left != 0),
        std::uint8_t(left == 0), state.xer_so};
}

void Dispatch(GuestMemory& memory, VirtualCalls& calls, Registers& state)
{
    state.r[11] = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(-2094792704));
    state.r[11] = memory.ReadU32(Address(state.r[11] + 25184u));
    CompareSigned(state, state.r[11]);
    if (!state.cr6.eq)
    {
        state.r[11] = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(-2093547520));
        state.r[12] = std::uint64_t{1} << 44;
        state.r[11] = memory.ReadU32(Address(state.r[11] - 28464u));
        state.r[11] = memory.ReadU32(Address(state.r[11] + 3040u));
        state.r[11] = ReadU64(memory, Address(state.r[11] + 4u));
        state.r[11] &= state.r[12];
        CompareUnsigned64(state, state.r[11]);
        if (!state.cr6.eq) return;
    }
    state.r[11] = memory.ReadU32(Address(state.r[3]));
    state.r[5] = 760u;
    state.r[11] = memory.ReadU32(Address(state.r[11] + 4u));
    state.ctr = state.r[11];
    calls.Call(Address(state.ctr) & ~GuestAddress{3}, memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    VirtualCalls& virtual_calls, Registers& registers)
{
    if (entry != 0x82479058u) return false;
    Dispatch(memory, virtual_calls, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_config_dispatch_gate
