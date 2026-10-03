#include "lo_semantics/legacy_token_float_cursor.h"

#include "lo_semantics/metadata_option_match.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/registered_metadata_string.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_token_float_cursor
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{
    return state.integer.r[index];
}

void CompareZero(Registers& state, std::uint64_t value)
{
    const auto word = static_cast<std::uint32_t>(value);
    state.integer.cr6 = {0, std::uint8_t(word != 0u),
        std::uint8_t(word == 0u), state.integer.xer_so};
}

void CallFind(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state)
{
    auto& integer = state.integer;
    integer.lr = 0x82297350u;
    InvalidParameterCall call{};
    for (unsigned index = 0; index < 8u; ++index)
        call.arguments[index] = R(state, index + 3u);
    call.thread_environment = R(state, 13);
    metadata_option_match::FrameRegisters frame{};
    frame.lr = integer.lr;
    for (unsigned index = 0; index < 7u; ++index)
        frame.r25_through_r31[index] = R(state, index + 25u);
    std::uint64_t result = 0;
    (void)metadata_option_match::Apply(0x82297390u, memory,
        thread_services, invalid_services, call, integer.sp, frame,
        result);
    for (unsigned index = 0; index < 8u; ++index)
        R(state, index + 3u) = call.arguments[index];
    R(state, 3) = result;
    R(state, 13) = call.thread_environment;
    for (unsigned index = 0; index < 7u; ++index)
        R(state, index + 25u) = frame.r25_through_r31[index];
    integer.lr = frame.lr;
    R(state, 12) = frame.lr;
}

void CallLength(GuestMemory& memory, Registers& state)
{
    auto& integer = state.integer;
    integer.lr = 0x8229736cu;
    const auto length = registered_metadata_string::Utf16Length(memory,
        R(state, 3));
    R(state, 10) = 0u;
    R(state, 11) = length + 1u;
    R(state, 3) = length;
    integer.xer_ca = 0;
    integer.cr0 = {0, 0, 1, integer.xer_so};
}

void Convert(Registers& state, NumberServices& numbers)
{
    R(state, 4) = 0u;
    state.f1_bits = std::bit_cast<std::uint64_t>(numbers.Convert(
        R(state, 3), R(state, 4)));
}

void ReadFloat(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    NumberServices& numbers, Registers& state)
{
    auto& integer = state.integer;
    const auto caller_sp = integer.sp;
    R(state, 12) = integer.lr;
    WriteU64(memory, Address(caller_sp - 32u), R(state, 29));
    WriteU64(memory, Address(caller_sp - 24u), R(state, 30));
    WriteU64(memory, Address(caller_sp - 16u), R(state, 31));
    memory.WriteU32(Address(caller_sp - 8u), Address(R(state, 12)));
    memory.WriteU32(Address(caller_sp - 112u), Address(caller_sp));
    integer.sp -= 112u;
    R(state, 31) = R(state, 4);
    R(state, 29) = R(state, 5);
    CallFind(memory, thread_services, invalid_services, state);
    R(state, 30) = R(state, 3);
    CompareZero(state, R(state, 30));
    if (!integer.cr6.eq)
    {
        R(state, 3) = R(state, 31);
        CallLength(memory, state);
        R(state, 11) = WordRotateMask(R(state, 3), 1, 0xfffffffeu);
        R(state, 3) = R(state, 11) + R(state, 30);
        integer.lr = 0x82297378u;
        Convert(state, numbers);
        constexpr std::uint32_t FlushMask = 0x8040u;
        if (state.cached_fp_control & FlushMask)
        {
            state.cached_fp_control &= ~FlushMask;
            numbers.SetHostFpControl(state.cached_fp_control);
        }
        const auto parsed = std::bit_cast<double>(state.f1_bits);
        const auto rounded = static_cast<float>(parsed);
        state.f0_bits = std::bit_cast<std::uint64_t>(
            static_cast<double>(rounded));
        memory.WriteU32(Address(R(state, 29)),
            std::bit_cast<std::uint32_t>(rounded));
        R(state, 3) = 1u;
    }
    integer.sp += 112u;
    R(state, 29) = ReadU64(memory, Address(integer.sp - 32u));
    R(state, 30) = ReadU64(memory, Address(integer.sp - 24u));
    R(state, 31) = ReadU64(memory, Address(integer.sp - 16u));
    R(state, 12) = memory.ReadU32(Address(integer.sp - 8u));
    integer.lr = R(state, 12);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services,
    NumberServices& numbers, Registers& registers)
{
    switch (entry)
    {
    case 0x82297338u:
        ReadFloat(memory, thread_services, invalid_services, numbers,
            registers);
        return true;
    case 0x822974a8u:
        Convert(registers, numbers);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_token_float_cursor
