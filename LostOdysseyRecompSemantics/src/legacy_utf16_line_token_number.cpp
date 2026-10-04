#include "lo_semantics/legacy_utf16_line_token_number.h"

#include "lo_semantics/manager_metadata_parsing.h"
#include "lo_semantics/metadata_option_match.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/registered_metadata_string.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_utf16_line_token_number
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{ return index == 1u ? state.sp : state.r[index]; }

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

void ReadLine(GuestMemory& memory, Registers& state)
{
    WriteU64(memory, Address(state.sp - 16u), R(state, 30));
    WriteU64(memory, Address(state.sp - 8u), R(state, 31));
    R(state, 31) = 0u;
    memory.WriteU32(Address(state.sp + 36u), Address(R(state, 5)));
    R(state, 6) = 256u;
    R(state, 5) = 0u;
    R(state, 8) = 0u;
    R(state, 7) = 0u;
    memory.WriteU16(Address(R(state, 4)), 0u);
    R(state, 11) = memory.ReadU32(Address(R(state, 3)));
    R(state, 11) = memory.ReadU16(Address(R(state, 11)));
    CompareUnsigned(state, R(state, 11), 0u);
    if (state.cr6.eq) goto at_delimiter;

read_character:
    R(state, 10) = memory.ReadU32(Address(R(state, 3)));
    R(state, 9) = memory.ReadU16(Address(R(state, 10)));
    R(state, 11) = R(state, 9);
    CompareUnsigned(state, R(state, 11), 10u);
    if (state.cr6.eq) goto at_delimiter;
    CompareUnsigned(state, R(state, 11), 13u);
    if (state.cr6.eq) goto at_delimiter;
    R(state, 6) -= 1u;
    CompareSigned(state, R(state, 6), 0);
    if (!state.cr6.gt) goto at_delimiter;
    CompareSigned(state, R(state, 8), 0);
    if (!state.cr6.eq) goto count_character;
    CompareUnsigned(state, R(state, 11), 47u);
    if (state.cr6.eq)
    {
        R(state, 30) = memory.ReadU16(Address(R(state, 10) + 2u));
        CompareUnsigned(state, R(state, 30), 47u);
        if (state.cr6.eq) R(state, 7) = 1u;
    }
    CompareUnsigned(state, R(state, 11), 124u);
    if (state.cr6.eq) goto at_delimiter;

count_character:
    R(state, 11) -= 34u;
    R(state, 5) = 1u;
    R(state, 11) = std::countl_zero(static_cast<std::uint32_t>(R(state, 11)));
    CompareSigned(state, R(state, 7), 0);
    R(state, 11) = WordRotateMask(R(state, 11), 27u, 1u);
    R(state, 8) = R(state, 11) ^ R(state, 8);
    if (!state.cr6.eq)
        R(state, 11) = R(state, 10) + 2u;
    else
    {
        memory.WriteU16(Address(R(state, 4)),
            static_cast<std::uint16_t>(R(state, 9)));
        R(state, 4) += 2u;
        R(state, 11) = memory.ReadU32(Address(R(state, 3))) + 2u;
    }
    memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
    R(state, 11) = static_cast<std::uint32_t>(R(state, 11));
    R(state, 11) = memory.ReadU16(Address(R(state, 11)));
    CompareUnsigned(state, R(state, 11), 0u);
    if (!state.cr6.eq) goto read_character;

at_delimiter:
    R(state, 10) = memory.ReadU32(Address(R(state, 3)));
    R(state, 11) = memory.ReadU16(Address(R(state, 10)));
    CompareUnsigned(state, R(state, 11), 10u);
    if (state.cr6.eq) goto consume_delimiter;
    CompareUnsigned(state, R(state, 11), 13u);
    if (state.cr6.eq) goto consume_delimiter;
    CompareUnsigned(state, R(state, 11), 124u);
    if (!state.cr6.eq) goto finish;

consume_delimiter:
    R(state, 11) = R(state, 10) + 2u;
    memory.WriteU32(Address(R(state, 3)), Address(R(state, 11)));
    goto at_delimiter;

finish:
    memory.WriteU16(Address(R(state, 4)), 0u);
    R(state, 11) = memory.ReadU32(Address(R(state, 3)));
    R(state, 11) = memory.ReadU16(Address(R(state, 11)));
    CompareUnsigned(state, R(state, 11), 0u);
    if (state.cr6.eq)
    {
        CompareSigned(state, R(state, 5), 0);
        R(state, 3) = 0u;
        if (!state.cr6.eq) R(state, 3) = 1u;
    }
    else R(state, 3) = 1u;
    R(state, 30) = ReadU64(memory, Address(state.sp - 16u));
    R(state, 31) = ReadU64(memory, Address(state.sp - 8u));
}

void CallFind(GuestMemory& memory, CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state)
{
    state.lr = 0x822988f8u;
    const bool scanned_text = Address(R(state, 3)) != 0u &&
        Address(R(state, 4)) != 0u;
    InvalidParameterCall call{};
    for (unsigned index = 0; index < 8u; ++index)
        call.arguments[index] = R(state, index + 3u);
    call.thread_environment = R(state, 13);
    metadata_option_match::FrameRegisters frame{};
    frame.lr = state.lr;
    for (unsigned index = 0; index < 7u; ++index)
        frame.r25_through_r31[index] = R(state, index + 25u);
    std::uint64_t result = 0;
    (void)metadata_option_match::Apply(0x82297390u, memory,
        thread_services, invalid_services, call, state.sp, frame, result);
    for (unsigned index = 0; index < 8u; ++index)
        R(state, index + 3u) = call.arguments[index];
    R(state, 3) = result;
    // A failed scan of two live strings exits 82297390 after loading the
    // terminating UTF-16 zero into r11. The accepted find interface does not
    // expose that volatile register to its caller.
    if (scanned_text && Address(result) == 0u) R(state, 11) = 0u;
    R(state, 13) = call.thread_environment;
    for (unsigned index = 0; index < 7u; ++index)
        R(state, index + 25u) = frame.r25_through_r31[index];
    state.lr = frame.lr;
    R(state, 12) = frame.lr;
}

void CallLength(GuestMemory& memory, Registers& state)
{
    state.lr = 0x82298914u;
    const auto length = registered_metadata_string::Utf16Length(
        memory, R(state, 3));
    R(state, 10) = 0u;
    R(state, 11) = length + 1u;
    R(state, 3) = length;
    state.xer_ca = 0;
    state.cr0 = {0, 0, 1, state.xer_so};
}

void CallParseTen(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state)
{
    state.lr = 0x82298920u;
    manager_metadata_parsing::FrameRegisters frame{};
    frame.lr = state.lr;
    frame.r13 = R(state, 13);
    frame.r8 = R(state, 8);
    frame.r9 = R(state, 9);
    for (unsigned index = 0; index < 9u; ++index)
        frame.r23_through_r31[index] = R(state, index + 23u);
    std::uint64_t result = 0;
    if (!manager_metadata_parsing::Apply(0x82376f98u, memory,
        thread_services, invalid_services, R(state, 3), R(state, 4),
        R(state, 5), R(state, 6), R(state, 7), state.sp, frame, result))
        throw std::runtime_error("accepted 82376F98 entry missing");
    R(state, 3) = result;
    R(state, 13) = frame.r13;
    R(state, 8) = frame.r8;
    R(state, 9) = frame.r9;
    for (unsigned index = 0; index < 9u; ++index)
        R(state, index + 23u) = frame.r23_through_r31[index];
    state.lr = frame.lr;
    R(state, 12) = frame.lr;
}

void ReadNamedNumber(GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& state)
{
    const auto caller_sp = state.sp;
    R(state, 12) = state.lr;
    for (unsigned index = 29u; index <= 31u; ++index)
        WriteU64(memory, Address(caller_sp - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(caller_sp - 8u), Address(R(state, 12)));
    memory.WriteU32(Address(caller_sp - 112u), Address(caller_sp));
    state.sp -= 112u;
    R(state, 31) = R(state, 4);
    R(state, 29) = R(state, 5);
    CallFind(memory, thread_services, invalid_services, state);
    R(state, 30) = R(state, 3);
    CompareUnsigned(state, R(state, 30), 0u);
    if (!state.cr6.eq)
    {
        R(state, 3) = R(state, 31);
        CallLength(memory, state);
        R(state, 11) = WordRotateMask(R(state, 3), 1u, 0xfffffffeu);
        R(state, 3) = R(state, 11) + R(state, 30);
        CallParseTen(memory, thread_services, invalid_services, state);
        R(state, 11) = R(state, 3);
        R(state, 3) = 1u;
        memory.WriteU32(Address(R(state, 29)), Address(R(state, 11)));
    }
    state.sp += 112u;
    for (unsigned index = 29u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& registers)
{
    switch (entry)
    {
    case 0x82295ee0u: ReadLine(memory, registers); return true;
    case 0x822988e0u:
        ReadNamedNumber(memory, thread_services, invalid_services,
            registers);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_utf16_line_token_number
