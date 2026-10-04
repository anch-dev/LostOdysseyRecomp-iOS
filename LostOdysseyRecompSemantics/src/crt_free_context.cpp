#include "lo_semantics/crt_free_context.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::crt_free_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

void CompareUnsigned(Registers& state, std::uint64_t left)
{
    const auto word = static_cast<std::uint32_t>(left);
    state.cr6 = {0u, std::uint8_t(word != 0u),
        std::uint8_t(word == 0u), state.xer_so};
}

void CompareSignedCr0(Registers& state, std::uint64_t left)
{
    const auto word = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    state.cr0 = {std::uint8_t(word < 0), std::uint8_t(word > 0),
        std::uint8_t(word == 0), state.xer_so};
}

void FreeRecord(GuestMemory& memory, LowerCalls& lower,
    Registers& state)
{
    state.r[12] = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r[12]));
    WriteU64(memory, Address(state.sp - 16u), state.r[31]);
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    state.r[31] = state.r[3];
    CompareUnsigned(state, state.r[31]);
    if (!state.cr6.eq)
    {
        state.lr = 0x823adde0u;
        state.r[11] = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(-2094792704));
        state.r[3] = memory.ReadU32(Address(state.r[11] + 22280u));
        state.r[4] = 0u;
        state.r[5] = state.r[31];
        state.lr = 0x823addecU;
        lower.Call(0x823ade28u, memory, state);
        CompareSignedCr0(state, state.r[3]);
        if (state.cr0.eq)
        {
            state.lr = 0x823addf8u;
            lower.Call(0x82b7fd78u, memory, state);
            state.r[31] = state.r[3];
            state.lr = 0x823ade00u;
            lower.Call(0x822ca100u, memory, state);
            state.lr = 0x823ade04u;
            lower.Call(0x82b7fd10u, memory, state);
            memory.WriteU32(Address(state.r[31]), Address(state.r[3]));
        }
    }
    state.sp += 96u;
    state.r[12] = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r[12];
    state.r[31] = ReadU64(memory, Address(state.sp - 16u));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    LowerCalls& lower, Registers& registers)
{
    if (entry != 0x823addc0u) return false;
    FreeRecord(memory, lower, registers);
    return true;
}
} // namespace lo::semantic::gpu::crt_free_context
