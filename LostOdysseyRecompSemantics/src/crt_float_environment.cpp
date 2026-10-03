#include "lo_semantics/crt_float_environment.h"

#include "lo_semantics/field_arithmetic.h"
#include "lo_semantics/invalid_parameter.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_float_environment
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void CompareUnsigned(Condition& condition, std::uint64_t value,
    std::uint32_t other, std::uint8_t so)
{
    const auto word = static_cast<std::uint32_t>(value);
    condition = {std::uint8_t(word < other), std::uint8_t(word > other),
        std::uint8_t(word == other), so};
}

void CompareSigned(Condition& condition, std::uint64_t value,
    std::int32_t other, std::uint8_t so)
{
    const auto word = static_cast<std::int32_t>(value);
    condition = {std::uint8_t(word < other), std::uint8_t(word > other),
        std::uint8_t(word == other), so};
}

void EnterFilterFrame(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(state.sp - 16u), R(state, 31));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
}

void LeaveFilterFrame(GuestMemory& memory, Registers& state)
{
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void FilterException(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    EnterFilterFrame(memory, state);
    R(state, 11) = 0xffffffff82000000ull;
    R(state, 31) = R(state, 3);
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + 2288u));
    R(state, 11) = memory.ReadU32(Address(R(state, 11)));
    CompareUnsigned(state.cr0, R(state, 11), 0, state.xer_so);
    if (!state.cr0.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 11) + 24u));
        R(state, 4) = 0;
        R(state, 3) = 10;
        state.ctr = R(state, 11);
        state.lr = 0x827cb0ecu;
        native.CallDebugMonitor(Address(state.ctr) & ~GuestAddress{3},
            memory, state);
    }
    else
        R(state, 3) = 0;

    CompareUnsigned(state.cr6, R(state, 3), 0, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 11) = 0xffffffff83370000ull;
        R(state, 10) = memory.ReadU32(Address(R(state, 11) + 18540u));
        CompareUnsigned(state.cr6, R(state, 10), 0, state.xer_so);
        if (!state.cr6.eq)
        {
            R(state, 3) = R(state, 31);
            R(state, 11) = static_cast<std::uint32_t>(R(state, 10));
            state.ctr = R(state, 11);
            state.lr = 0x827cb11cu;
            native.CallExceptionHandler(Address(state.ctr) & ~GuestAddress{3},
                memory, state);
            CompareSigned(state.cr6, R(state, 3), -1, state.xer_so);
            R(state, 3) = ~std::uint64_t{0};
            if (state.cr6.eq)
            {
                LeaveFilterFrame(memory, state);
                return;
            }
        }
    }
    R(state, 3) = 0;
    LeaveFilterFrame(memory, state);
}

void ExchangeExceptionHandler(GuestMemory& memory, Registers& state)
{
    lo::semantic::field_arithmetic::Registers lower{};
    lower.r3 = R(state, 3);
    lower.r4 = R(state, 4);
    lower.r5 = R(state, 5);
    lower.r8 = R(state, 8);
    lower.r9 = R(state, 9);
    lower.r10 = R(state, 10);
    lower.r11 = R(state, 11);
    if (!lo::semantic::field_arithmetic::Apply(0x827cafe0u, lower, memory))
        throw std::logic_error("missing accepted exception-handler exchange");
    R(state, 3) = lower.r3;
    R(state, 4) = lower.r4;
    R(state, 5) = lower.r5;
    R(state, 8) = lower.r8;
    R(state, 9) = lower.r9;
    R(state, 10) = lower.r10;
    R(state, 11) = lower.r11;
}

void ReportFatalRuntimeError(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    memory.WriteU32(Address(state.sp - 2816u), Address(state.sp));
    state.sp -= 2816u;
    R(state, 5) = 2624;
    R(state, 4) = 0;
    R(state, 3) = state.sp + 176u;
    state.lr = 0x82b7ff24u;
    R(state, 3) = FillGuestMemory(memory, Address(R(state, 3)),
        Address(R(state, 4)), Address(R(state, 5)));

    R(state, 11) = state.sp + 96u;
    R(state, 9) = 0;
    R(state, 10) = 10;
    state.ctr = R(state, 10);
    do
    {
        WriteU64(memory, Address(R(state, 11)), R(state, 9));
        R(state, 11) += 8u;
        --state.ctr;
    } while (static_cast<std::uint32_t>(state.ctr) != 0);

    R(state, 11) = 0xffffffffc0000000ull;
    R(state, 3) = 0;
    R(state, 11) |= 13u;
    memory.WriteU32(Address(state.sp + 96u), Address(R(state, 11)));
    R(state, 11) = memory.ReadU32(Address(state.sp + 2808u));
    memory.WriteU32(Address(state.sp + 108u), Address(R(state, 11)));
    R(state, 11) = state.sp + 96u;
    memory.WriteU32(Address(state.sp + 80u), Address(R(state, 11)));
    R(state, 11) = state.sp + 176u;
    memory.WriteU32(Address(state.sp + 84u), Address(R(state, 11)));

    state.lr = 0x82b7ff6cu;
    ExchangeExceptionHandler(memory, state);
    R(state, 3) = state.sp + 80u;
    state.lr = 0x82b7ff74u;
    FilterException(memory, native, state);
    CompareUnsigned(state.cr0, R(state, 3), 0, state.xer_so);
    if (state.cr0.eq)
    {
        R(state, 3) = 2;
        state.lr = 0x82b7ff84u;
        R(state, 10) = 0xffffffff83380000ull;
        R(state, 11) = 0;
        ClearInvalidParameterState(memory);
    }
    R(state, 3) = 30;
    state.lr = 0x82b7ff8cu;
    native.BugCheck(memory, state);
    // The pinned PPC body has no epilogue after the native bug-check import.
}

void FindLastByte(GuestMemory& memory, Registers& state)
{
    R(state, 5) = memory.ReadU8(Address(R(state, 3)));
    R(state, 9) = R(state, 3);
    CompareSigned(state.cr6, R(state, 4), 0, state.xer_so);
    CompareSigned(state.cr0, R(state, 5),
        static_cast<std::int32_t>(R(state, 4)), state.xer_so);
    if (state.cr6.eq)
    {
        while (!state.cr0.eq)
        {
            R(state, 3) += 1u;
            R(state, 5) = memory.ReadU8(Address(R(state, 3)));
            CompareSigned(state.cr0, R(state, 5), 0, state.xer_so);
        }
        return;
    }

    R(state, 3) = 0;
    for (;;)
    {
        while (state.cr0.eq)
        {
            R(state, 3) = R(state, 9);
            R(state, 9) += 1u;
            R(state, 5) = memory.ReadU8(Address(R(state, 9)));
            CompareSigned(state.cr0, R(state, 4),
                static_cast<std::int32_t>(R(state, 5)), state.xer_so);
        }
        CompareSigned(state.cr6, R(state, 5), 0, state.xer_so);
        if (state.cr6.eq)
            return;
        R(state, 9) += 1u;
        R(state, 5) = memory.ReadU8(Address(R(state, 9)));
        CompareSigned(state.cr0, R(state, 4),
            static_cast<std::int32_t>(R(state, 5)), state.xer_so);
    }
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& registers)
{
    switch (entry)
    {
    case 0x82b7ff08u:
        ReportFatalRuntimeError(memory, native, registers); return true;
    case 0x82b7e580u:
        FindLastByte(memory, registers); return true;
    case 0x827cb0b0u:
        FilterException(memory, native, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_float_environment
