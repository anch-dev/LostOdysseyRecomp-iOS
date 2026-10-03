#include "lo_semantics/legacy_utf16_prefix_cursor.h"

#include "lo_semantics/legacy_character_classification.h"
#include "lo_semantics/metadata_option_match.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/registered_metadata_string.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_utf16_prefix_cursor
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

void CompareUnsigned(Registers& state, std::uint64_t value,
    std::uint32_t bound)
{
    const auto word = static_cast<std::uint32_t>(value);
    state.cr6 = {std::uint8_t(word < bound), std::uint8_t(word > bound),
        std::uint8_t(word == bound), state.xer_so};
}

void CompareSignedZero(Registers& state, std::uint64_t value)
{
    const auto word = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(value));
    state.cr6 = {std::uint8_t(word < 0), std::uint8_t(word > 0),
        std::uint8_t(word == 0), state.xer_so};
}

void CallLength(GuestMemory& memory, Registers& state,
    std::uint64_t return_address)
{
    state.lr = return_address;
    const auto length = registered_metadata_string::Utf16Length(memory,
        R(state, 3));
    R(state, 11) = length + 1u;
    R(state, 10) = 0u;
    R(state, 3) = length;
    state.xer_ca = 0;
    state.cr0 = {0, 0, 1, state.xer_so};
}

void CallCompare(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state)
{
    constexpr std::uint64_t return_address = 0x8229679cu;
    state.lr = return_address;
    InvalidParameterCall call{};
    for (unsigned index = 0; index < 8u; ++index)
        call.arguments[index] = R(state, index + 3u);
    call.thread_environment = R(state, 13);
    metadata_option_match::FrameRegisters frame{};
    frame.lr = state.lr;
    for (unsigned index = 0; index < 7u; ++index)
        frame.r25_through_r31[index] = R(state, index + 25u);
    const bool compared = static_cast<std::uint32_t>(R(state, 5)) != 0u;
    std::uint64_t result = 0;
    (void)metadata_option_match::Apply(0x82296858u, memory,
        thread_services, invalid_services, call, state.sp, frame, result);
    for (unsigned index = 0; index < 8u; ++index)
        R(state, index + 3u) = call.arguments[index];
    R(state, 3) = result;
    R(state, 13) = call.thread_environment;
    for (unsigned index = 0; index < 7u; ++index)
        R(state, index + 25u) = frame.r25_through_r31[index];
    state.lr = frame.lr;
    R(state, 12) = frame.lr;
    R(state, 11) = result;
    if (compared)
    {
        state.xer_ca = 1;
        const bool zero = static_cast<std::uint32_t>(R(state, 5)) == 0u ||
            static_cast<std::uint32_t>(R(state, 10)) == 0u;
        state.cr0 = {0, std::uint8_t(!zero), std::uint8_t(zero),
            state.xer_so};
    }
}

void CallClassification(GuestMemory& memory, Registers& state,
    std::uint64_t return_address)
{
    state.lr = return_address;
    legacy_character_classification::Registers lower{state.sp, state.lr,
        R(state, 3), R(state, 11), R(state, 12), R(state, 31),
        state.xer_so, {state.cr6.lt, state.cr6.gt, state.cr6.eq,
            state.cr6.so}};
    (void)legacy_character_classification::Apply(0x82296938u, memory,
        lower);
    state.sp = lower.sp;
    state.lr = lower.lr;
    R(state, 3) = lower.r3;
    R(state, 11) = lower.r11;
    R(state, 12) = lower.r12;
    R(state, 31) = lower.r31;
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq,
        lower.cr6.so};
}

void SkipWhitespace(GuestMemory& memory, Registers& state)
{
    for (;;)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 31)));
        R(state, 10) = memory.ReadU16(Address(R(state, 11)));
        CompareUnsigned(state, R(state, 10), 32u);
        if (!state.cr6.eq)
        {
            CompareUnsigned(state, R(state, 10), 9u);
            if (!state.cr6.eq) return;
        }
        R(state, 11) += 2u;
        memory.WriteU32(Address(R(state, 31)), Address(R(state, 11)));
    }
}

void ReadPrefix(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state)
{
    const auto caller_sp = state.sp;
    R(state, 12) = state.lr;
    memory.WriteU32(Address(caller_sp - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(caller_sp - 24u), R(state, 30));
    WriteU64(memory, Address(caller_sp - 16u), R(state, 31));
    memory.WriteU32(Address(caller_sp - 112u), Address(caller_sp));
    state.sp -= 112u;
    R(state, 31) = R(state, 3);
    R(state, 30) = R(state, 4);

    SkipWhitespace(memory, state);
    R(state, 3) = R(state, 30);
    CallLength(memory, state, 0x82296788u);
    R(state, 11) = memory.ReadU32(Address(R(state, 31)));
    R(state, 5) = R(state, 3);
    R(state, 4) = R(state, 30);
    R(state, 3) = R(state, 11);
    CallCompare(memory, thread_services, invalid_services, state);
    CompareSignedZero(state, R(state, 3));
    if (state.cr6.eq)
    {
        R(state, 3) = R(state, 30);
        CallLength(memory, state, 0x822967acu);
        R(state, 10) = memory.ReadU32(Address(R(state, 31)));
        R(state, 11) = WordRotateMask(R(state, 3), 1, 0xfffffffeu);
        R(state, 11) += R(state, 10);
        memory.WriteU32(Address(R(state, 31)), Address(R(state, 11)));
        R(state, 3) = memory.ReadU16(Address(R(state, 11)));
        CallClassification(memory, state, 0x822967c4u);
        CompareSignedZero(state, R(state, 3));
        if (state.cr6.eq)
        {
            SkipWhitespace(memory, state);
            R(state, 3) = 1u;
        }
        else
        {
            R(state, 3) = R(state, 30);
            CallLength(memory, state, 0x82296800u);
            R(state, 11) = R(state, 3);
            R(state, 10) = memory.ReadU32(Address(R(state, 31)));
            R(state, 11) = R(state, 10) -
                WordRotateMask(R(state, 11), 1, 0xfffffffeu);
            memory.WriteU32(Address(R(state, 31)), Address(R(state, 11)));
            R(state, 3) = 0u;
        }
    }
    else
    {
        R(state, 3) = 0u;
    }

    state.sp += 112u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    R(state, 30) = ReadU64(memory, Address(state.sp - 24u));
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& registers)
{
    if (entry != 0x82296740u) return false;
    ReadPrefix(memory, thread_services, invalid_services, registers);
    return true;
}
} // namespace lo::semantic::gpu::legacy_utf16_prefix_cursor
