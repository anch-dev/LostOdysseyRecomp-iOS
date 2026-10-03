#include "lo_semantics/crt_formatter.h"

#include "lo_semantics/crt_format_dispatch.h"
#include "lo_semantics/read_only_fields.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_formatter
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t W(std::uint64_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t SW(std::uint64_t value) { return static_cast<std::int32_t>(value); }
std::uint16_t H(std::uint64_t value) { return static_cast<std::uint16_t>(value); }

void CompareUnsigned(Condition& cr, std::uint64_t left,
    std::uint64_t right, std::uint8_t so)
{
    const auto a = W(left), b = W(right);
    cr = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), so};
}

void CompareSigned(Condition& cr, std::uint64_t left,
    std::int32_t right, std::uint8_t so)
{
    const auto a = SW(left);
    cr = {std::uint8_t(a < right), std::uint8_t(a > right),
        std::uint8_t(a == right), so};
}

void CompareSigned64(Condition& cr, std::uint64_t left,
    std::int64_t right, std::uint8_t so)
{
    const auto a = static_cast<std::int64_t>(left);
    cr = {std::uint8_t(a < right), std::uint8_t(a > right),
        std::uint8_t(a == right), so};
}

void AddImmediate(Registers& state, unsigned index, std::int32_t value)
{
    const auto before = R(state, index);
    state.xer_ca = std::uint8_t(value < 0 ?
        W(before) >= static_cast<std::uint32_t>(-value) :
        W(before) > 0xffffffffu - static_cast<std::uint32_t>(value));
    R(state, index) += static_cast<std::uint64_t>(value);
    CompareSigned(state.cr0, R(state, index), 0, state.xer_so);
}

void SaveFrame(GuestMemory& memory, Registers& state,
    unsigned first, std::uint32_t frame)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    for (unsigned index = first; index <= 31; ++index)
        WriteU64(memory, Address(state.sp - 16u - 8u * (31u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - frame), Address(state.sp));
    state.sp -= frame;
}

void RestoreFrame(GuestMemory& memory, Registers& state,
    unsigned first, std::uint32_t frame)
{
    state.sp += frame;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    for (unsigned index = first; index <= 31; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 16u - 8u * (31u - index)));
}

void CallOutput(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    if (!crt_wide_stream_output::ApplyAcceptedCallee(entry, memory,
            dependencies.output, state))
        throw std::logic_error("unrecovered formatter output callee");
}

void CallSupport(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    crt_formatting_support::Registers call{};
    call.sp = state.sp; call.lr = state.lr;
    call.r3 = R(state, 3); call.r4 = R(state, 4);
    call.r5 = R(state, 5); call.r6 = R(state, 6);
    call.r7 = R(state, 7); call.r8 = R(state, 8);
    call.r9 = R(state, 9); call.r10 = R(state, 10);
    call.r11 = R(state, 11); call.r12 = R(state, 12);
    call.r13 = R(state, 13); call.r31 = R(state, 31);
    call.xer_so = state.xer_so;
    call.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.so};
    call.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    if (!crt_formatting_support::Apply(entry, memory,
            dependencies.support, call))
        throw std::logic_error("unrecovered formatter support callee");
    state.sp = call.sp; state.lr = call.lr;
    R(state, 3) = call.r3; R(state, 4) = call.r4;
    R(state, 5) = call.r5; R(state, 6) = call.r6;
    R(state, 7) = call.r7; R(state, 8) = call.r8;
    R(state, 9) = call.r9; R(state, 10) = call.r10;
    R(state, 11) = call.r11; R(state, 12) = call.r12;
    R(state, 13) = call.r13; R(state, 31) = call.r31;
    state.xer_so = call.xer_so;
    state.cr0 = {call.cr0.lt, call.cr0.gt, call.cr0.eq, call.cr0.so};
    state.cr6 = {call.cr6.lt, call.cr6.gt, call.cr6.eq, call.cr6.so};
}

void CallCharacterFlag(GuestMemory& memory, Registers& state)
{
    read_only_fields::Registers call{R(state, 3), R(state, 4),
        R(state, 5), R(state, 8), R(state, 9), R(state, 10),
        R(state, 11), R(state, 13), R(state, 18)};
    if (!read_only_fields::Apply(0x82b7a680u, call, memory))
        throw std::logic_error("unrecovered CRT character flag");
    R(state, 3) = call.r3; R(state, 4) = call.r4;
    R(state, 5) = call.r5; R(state, 8) = call.r8;
    R(state, 9) = call.r9; R(state, 10) = call.r10;
    R(state, 11) = call.r11; R(state, 13) = call.r13;
    R(state, 18) = call.r18;
}

void CallFloatSlot(GuestAddress slot, GuestMemory& memory,
    Dependencies dependencies, Registers& state, GuestAddress return_address,
    unsigned target_register)
{
    R(state, target_register) = memory.ReadU32(slot);
    state.ctr = R(state, target_register);
    state.lr = return_address;
    const auto target = Address(state.ctr) & ~GuestAddress{3};
    if (!crt_float_formatting::Apply(target, memory,
            dependencies.floating, state) &&
        !crt_format_dispatch::Apply(target, memory,
            dependencies.output.streams.locks, state))
        dependencies.dynamic.CallGuestFormatter(target, memory, state);
}

void CallRawAllocation(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    state.lr = 0x823198f4u;
    R(state, 3) = AllocateRawMemory(memory,
        dependencies.output.streams.raw, R(state, 3));
}

void CallFree(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    state.lr = 0x82319e0cu;
    R(state, 3) = FreeCrtRecord(memory, dependencies.allocation,
        R(state, 3), Address(R(state, 13)), Address(state.sp));
}

void FormatEntry(GuestMemory& memory, Dependencies dependencies,
    Registers& state);

class FormatEngine
{
public:
    FormatEngine(GuestMemory& memory, Dependencies dependencies,
        Registers& registers)
        : memory_(memory), dependencies_(dependencies), state_(registers) {}

    void Run();

private:
    GuestMemory& memory_;
    Dependencies dependencies_;
    Registers& state_;
    GuestAddress sp_ = 0;
    bool aborted_ = false;

    void NextCodeUnit();
    void ProcessToken();
    void ProcessSpecifier();
    void FormatCharacter(bool upper);
    void FormatCountedString();
    void FormatString(bool upper);
    void FormatCountOutput();
    void FormatFloat();
    void FormatInteger(GuestAddress branch);
    void EmitField();
    void EmitPadding(std::uint32_t code_unit, unsigned count,
        GuestAddress return_address);
    void EmitText(bool wide, GuestAddress text, unsigned count,
        GuestAddress return_address);
    void EmitCodeUnit(std::uint32_t code_unit, GuestAddress return_address);
    void RecoverError(GuestAddress return_address);
    void FailInvalid();
    [[nodiscard]] bool IsOutputError() const
    { return SW(memory_.ReadU32(sp_ + 80u)) == -1; }
};

void FormatEngine::RecoverError(GuestAddress return_address)
{
    CallOutput(0x82b7fd78u, memory_, dependencies_, state_, return_address);
}

void FormatEngine::FailInvalid()
{
    RecoverError(0x82319378u);
    R(state_, 11) = R(state_, 3);
    R(state_, 10) = 22;
    for (unsigned index = 3; index <= 7; ++index)
        R(state_, index) = 0;
    memory_.WriteU32(Address(R(state_, 11)), 22);
    CallOutput(0x82b7fec0u, memory_, dependencies_, state_, 0x8231939cu);
    R(state_, 3) = ~std::uint64_t{0};
    aborted_ = true;
}

void FormatEngine::EmitCodeUnit(std::uint32_t code_unit,
    GuestAddress return_address)
{
    R(state_, 5) = state_.sp + 80u;
    R(state_, 4) = R(state_, 21);
    R(state_, 3) = code_unit;
    CallOutput(0x82b84a08u, memory_, dependencies_, state_, return_address);
    R(state_, 22) = memory_.ReadU32(sp_ + 80u);
}

void FormatEngine::EmitPadding(std::uint32_t code_unit, unsigned count,
    GuestAddress return_address)
{
    for (unsigned remaining = count; remaining != 0; --remaining)
    {
        EmitCodeUnit(code_unit, return_address);
        CompareSigned(state_.cr6, R(state_, 22), -1, state_.xer_so);
        if (state_.cr6.eq)
            break;
    }
}

void FormatEngine::EmitText(bool wide, GuestAddress text, unsigned count,
    GuestAddress return_address)
{
    R(state_, 30) = count;
    R(state_, 31) = text;
    if (!count)
        return;
    do
    {
        if (wide)
            R(state_, 3) = memory_.ReadU16(Address(R(state_, 31)));
        else
            R(state_, 3) = memory_.ReadU8(Address(R(state_, 31)));
        R(state_, 5) = state_.sp + 80u;
        R(state_, 4) = R(state_, 21);
        R(state_, 30) -= 1u;
        CallOutput(0x82b84a08u, memory_, dependencies_, state_,
            return_address);
        R(state_, 22) = memory_.ReadU32(sp_ + 80u);
        R(state_, 31) += wide ? 2u : 1u;
        CompareSigned(state_.cr6, R(state_, 22), -1, state_.xer_so);
        if (state_.cr6.eq)
        {
            RecoverError(return_address);
            R(state_, 11) = memory_.ReadU32(Address(R(state_, 3)));
            CompareSigned(state_.cr6, R(state_, 11), 42, state_.xer_so);
            if (state_.cr6.eq)
                EmitCodeUnit(63, return_address);
            break;
        }
        CompareSigned(state_.cr6, R(state_, 30), 0, state_.xer_so);
    } while (state_.cr6.gt);
}

void FormatEngine::NextCodeUnit()
{
    R(state_, 29) = memory_.ReadU16(Address(R(state_, 15)));
    R(state_, 10) = R(state_, 29);
    CompareSigned(state_.cr0, R(state_, 10), 0, state_.xer_so);
    if (!state_.cr0.eq)
    {
        R(state_, 31) = memory_.ReadU32(sp_ + 104u);
        R(state_, 28) = 0;
        R(state_, 8) = memory_.ReadU32(sp_ + 108u);
        R(state_, 9) = memory_.ReadU32(sp_ + 100u);
    }
}

void FormatEngine::ProcessToken()
{
    R(state_, 15) += 2u;
    CompareSigned(state_.cr6, R(state_, 22), 0, state_.xer_so);
    if (state_.cr6.lt)
        return;
    CompareUnsigned(state_.cr6, R(state_, 10), 32u, state_.xer_so);
    if (state_.cr6.lt || W(R(state_, 10)) > 120u)
        R(state_, 11) = 0;
    else
    {
        CompareUnsigned(state_.cr6, R(state_, 10), 120u, state_.xer_so);
        R(state_, 11) = memory_.ReadU8(
            Address(R(state_, 9) + R(state_, 10) - 32u));
        R(state_, 11) = W(R(state_, 11)) & 15u;
    }
    R(state_, 11) = WordRotateMask(R(state_, 11), 3, 0xfffffff8u);
    R(state_, 11) += R(state_, 8);
    R(state_, 11) = memory_.ReadU8(Address(R(state_, 9) + R(state_, 11)));
    R(state_, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int8_t>(R(state_, 11))));
    state_.xer_ca = std::uint8_t(SW(R(state_, 11)) < 0 &&
        (W(R(state_, 11)) & 15u) != 0);
    R(state_, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(SW(R(state_, 11)) >> 4));
    CompareUnsigned(state_.cr6, R(state_, 11), 7u, state_.xer_so);
    memory_.WriteU32(sp_ + 108u, W(R(state_, 11)));
    if (state_.cr6.gt)
        return;

    R(state_, 12) = 0xffffffff820d0000ull;
    R(state_, 12) += 14032u;
    R(state_, 0) = WordRotateMask(R(state_, 11), 1, 0xfffffffeu);
    R(state_, 0) = memory_.ReadU16(Address(R(state_, 12) + R(state_, 0)));
    R(state_, 12) = 0xffffffff82320000ull;
    R(state_, 12) -= 27512u;
    R(state_, 12) += R(state_, 0);
    state_.ctr = R(state_, 12);
    switch (Address(state_.ctr))
    {
    case 0x8231963cu: // A literal code unit.
        memory_.WriteU32(sp_ + 108u, 0);
        R(state_, 5) = state_.sp + 80u;
        R(state_, 4) = R(state_, 21);
        R(state_, 3) = R(state_, 29);
        R(state_, 19) = 1;
        CallOutput(0x82b84a08u, memory_, dependencies_, state_, 0x82319650u);
        R(state_, 22) = memory_.ReadU32(sp_ + 80u);
        break;
    case 0x82319488u: // Start a conversion.
        R(state_, 16) = 0;
        memory_.WriteU32(sp_ + 96u, 0);
        R(state_, 14) = 0;
        R(state_, 17) = 0;
        R(state_, 25) = 0;
        R(state_, 23) = ~std::uint64_t{0};
        R(state_, 19) = 0;
        break;
    case 0x823194a8u: // Flags.
        switch (W(R(state_, 10)))
        {
        case '0': R(state_, 25) |= 8u; break;
        case '-': R(state_, 25) |= 4u; break;
        case '+': R(state_, 25) |= 1u; break;
        case '#': R(state_, 25) |= 128u; break;
        case ' ': R(state_, 25) |= 2u; break;
        default: break;
        }
        break;
    case 0x823194f8u: // Field width.
        if (W(R(state_, 10)) == '*')
        {
            R(state_, 11) = WordRotateMask(R(state_, 24) + 7u, 0,
                0xfffffff8u);
            R(state_, 24) = R(state_, 11) + 8u;
            R(state_, 14) = memory_.ReadU32(Address(R(state_, 24) - 4u));
            CompareSigned(state_.cr0, R(state_, 14), 0, state_.xer_so);
            if (state_.cr0.lt)
            {
                R(state_, 25) |= 4u;
                R(state_, 14) = 0u - R(state_, 14);
            }
        }
        else
            R(state_, 14) = W(R(state_, 14) * 10u + R(state_, 10) - 48u);
        break;
    case 0x82319534u:
        R(state_, 23) = 0;
        break;
    case 0x8231953cu: // Precision.
        if (W(R(state_, 10)) == '*')
        {
            R(state_, 11) = WordRotateMask(R(state_, 24) + 7u, 0,
                0xfffffff8u);
            R(state_, 24) = R(state_, 11) + 8u;
            R(state_, 23) = memory_.ReadU32(Address(R(state_, 24) - 4u));
            CompareSigned(state_.cr0, R(state_, 23), 0, state_.xer_so);
            if (state_.cr0.lt)
                R(state_, 23) = ~std::uint64_t{0};
        }
        else
            R(state_, 23) = W(R(state_, 23) * 10u + R(state_, 10) - 48u);
        break;
    case 0x82319574u: // Argument length.
        if (W(R(state_, 10)) == 'I')
        {
            R(state_, 11) = memory_.ReadU16(Address(R(state_, 15)));
            CompareUnsigned(state_.cr6, R(state_, 11), '6', state_.xer_so);
            if (state_.cr6.eq &&
                memory_.ReadU16(Address(R(state_, 15)) + 2u) == '4')
            {
                R(state_, 10) = '4';
                R(state_, 15) += 4u;
                R(state_, 25) |= 32768u;
            }
            else if (W(R(state_, 11)) == '3' &&
                memory_.ReadU16(Address(R(state_, 15)) + 2u) == '2')
            {
                R(state_, 10) = '2';
                R(state_, 15) += 4u;
                R(state_, 25) = WordRotateMask(R(state_, 25), 0,
                    0xffff7fffu);
            }
        }
        else if (W(R(state_, 10)) == 'h')
            R(state_, 25) |= 32u;
        else if (W(R(state_, 10)) == 'l')
        {
            R(state_, 11) = memory_.ReadU16(Address(R(state_, 15)));
            CompareUnsigned(state_.cr6, R(state_, 11), 'l', state_.xer_so);
            if (state_.cr6.eq)
            {
                R(state_, 15) += 2u;
                R(state_, 25) |= 4096u;
            }
            else
                R(state_, 25) |= 16u;
        }
        else if (W(R(state_, 10)) == 'w')
            R(state_, 25) |= 2048u;
        break;
    case 0x82319658u:
        ProcessSpecifier();
        break;
    default:
        throw std::logic_error("format parser jump table has an unknown target");
    }
}

void FormatEngine::FormatCharacter(bool upper)
{
    if (upper)
    {
        R(state_, 11) = W(R(state_, 25)) & 2096u;
        CompareSigned(state_.cr0, R(state_, 11), 0, state_.xer_so);
        if (state_.cr0.eq)
            R(state_, 25) |= 32u;
    }
    R(state_, 11) = WordRotateMask(R(state_, 24) + 7u, 0,
        0xfffffff8u);
    R(state_, 26) = 1;
    R(state_, 24) = R(state_, 11) + 8u;
    R(state_, 10) = WordRotateMask(R(state_, 25), 0, 0x20u);
    CompareSigned(state_.cr0, R(state_, 10), 0, state_.xer_so);
    R(state_, 19) = R(state_, 26);
    R(state_, 11) = memory_.ReadU32(Address(R(state_, 24) - 4u));
    R(state_, 11) = H(R(state_, 11));
    memory_.WriteU16(sp_ + 84u, H(R(state_, 11)));
    if (!state_.cr0.eq)
    {
        memory_.WriteU8(sp_ + 86u, static_cast<std::uint8_t>(R(state_, 11)));
        R(state_, 4) = state_.sp + 86u;
        R(state_, 11) = memory_.ReadU32(Address(R(state_, 20)));
        R(state_, 6) = R(state_, 20);
        R(state_, 3) = state_.sp + 128u;
        memory_.WriteU8(sp_ + 87u, 0);
        R(state_, 5) = memory_.ReadU32(Address(R(state_, 11)) + 172u);
        CallSupport(0x822a07a0u, memory_, dependencies_, state_,
            0x823196e4u);
        CompareSigned(state_.cr0, R(state_, 3), 0, state_.xer_so);
        if (state_.cr0.lt)
        {
            memory_.WriteU32(sp_ + 96u, 1);
            R(state_, 26) = 1;
        }
    }
    else
        memory_.WriteU16(sp_ + 128u, H(R(state_, 11)));
    R(state_, 27) = state_.sp + 128u;
}

void FormatEngine::FormatCountedString()
{
    R(state_, 11) = WordRotateMask(R(state_, 24) + 7u, 0,
        0xfffffff8u);
    R(state_, 24) = R(state_, 11) + 8u;
    R(state_, 11) = memory_.ReadU32(Address(R(state_, 24) - 4u));
    CompareUnsigned(state_.cr0, R(state_, 11), 0, state_.xer_so);
    if (!state_.cr0.eq)
    {
        R(state_, 10) = memory_.ReadU32(Address(R(state_, 11)) + 4u);
        CompareUnsigned(state_.cr0, R(state_, 10), 0, state_.xer_so);
        if (!state_.cr0.eq)
        {
            R(state_, 9) = WordRotateMask(R(state_, 25), 0, 0x800u);
            CompareSigned(state_.cr0, R(state_, 9), 0, state_.xer_so);
            R(state_, 27) = R(state_, 10);
            R(state_, 11) = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(static_cast<std::int16_t>(
                    memory_.ReadU16(Address(R(state_, 11))))));
            if (!state_.cr0.eq)
            {
                R(state_, 19) = 1;
                state_.xer_ca = std::uint8_t(SW(R(state_, 11)) < 0 &&
                    (W(R(state_, 11)) & 1u) != 0);
                R(state_, 11) = static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(SW(R(state_, 11)) >> 1));
                const auto shifted = R(state_, 11);
                R(state_, 26) = shifted + state_.xer_ca;
                state_.xer_ca = std::uint8_t(
                    W(R(state_, 26)) < W(shifted));
            }
            else
            {
                R(state_, 26) = R(state_, 11);
                R(state_, 19) = 0;
            }
            return;
        }
    }
    R(state_, 11) = 0xffffffff820d0000ull;
    R(state_, 27) = memory_.ReadU32(Address(R(state_, 11)) + 14296u);
    R(state_, 11) = R(state_, 27);
    R(state_, 10) = R(state_, 11);
    do
    {
        R(state_, 9) = memory_.ReadU8(Address(R(state_, 11)));
        R(state_, 11) += 1u;
        CompareUnsigned(state_.cr6, R(state_, 9), 0, state_.xer_so);
    } while (!state_.cr6.eq);
    R(state_, 11) = R(state_, 11) - R(state_, 10) - 1u;
    R(state_, 26) = WordRotateMask(R(state_, 11), 0, 0xffffffffu);
}

void FormatEngine::FormatString(bool upper)
{
    if (upper)
    {
        R(state_, 11) = W(R(state_, 25)) & 2096u;
        CompareSigned(state_.cr0, R(state_, 11), 0, state_.xer_so);
        if (state_.cr0.eq)
            R(state_, 25) |= 32u;
    }
    CompareSigned(state_.cr6, R(state_, 23), -1, state_.xer_so);
    R(state_, 30) = state_.cr6.eq ? 0x7fffffffu : R(state_, 23);
    R(state_, 11) = WordRotateMask(R(state_, 24) + 7u, 0,
        0xfffffff8u);
    R(state_, 10) = WordRotateMask(R(state_, 25), 0, 0x20u);
    CompareSigned(state_.cr0, R(state_, 10), 0, state_.xer_so);
    R(state_, 24) = R(state_, 11) + 8u;
    R(state_, 27) = memory_.ReadU32(Address(R(state_, 24) - 4u));
    CompareUnsigned(state_.cr6, R(state_, 27), 0, state_.xer_so);
    if (!state_.cr0.eq)
    {
        if (state_.cr6.eq)
        {
            R(state_, 11) = 0xffffffff820d0000ull;
            R(state_, 27) = memory_.ReadU32(Address(R(state_, 11)) + 14296u);
        }
        R(state_, 31) = R(state_, 27);
        R(state_, 26) = 0;
        CompareSigned(state_.cr6, R(state_, 30), 0, state_.xer_so);
        while (state_.cr6.gt)
        {
            R(state_, 11) = memory_.ReadU8(Address(R(state_, 31)));
            CompareUnsigned(state_.cr0, R(state_, 11), 0, state_.xer_so);
            if (state_.cr0.eq)
                break;
            R(state_, 3) = W(R(state_, 11)) & 0xffu;
            R(state_, 4) = R(state_, 20);
            state_.lr = 0x823197f8u;
            CallCharacterFlag(memory_, state_);
            CompareSigned(state_.cr0, R(state_, 3), 0, state_.xer_so);
            if (!state_.cr0.eq)
                R(state_, 31) += 1u;
            R(state_, 26) += 1u;
            R(state_, 31) += 1u;
            CompareUnsigned(state_.cr6, R(state_, 26), R(state_, 30),
                state_.xer_so);
            if (!state_.cr6.lt)
                break;
        }
        return;
    }
    if (state_.cr6.eq)
    {
        R(state_, 11) = 0xffffffff820d0000ull;
        R(state_, 27) = memory_.ReadU32(Address(R(state_, 11)) + 14300u);
    }
    R(state_, 19) = 1;
    R(state_, 11) = R(state_, 27);
    while (SW(R(state_, 30)) != 0)
    {
        R(state_, 10) = memory_.ReadU16(Address(R(state_, 11)));
        R(state_, 30) -= 1u;
        CompareUnsigned(state_.cr0, R(state_, 10), 0, state_.xer_so);
        if (state_.cr0.eq)
            break;
        R(state_, 11) += 2u;
        CompareSigned(state_.cr6, R(state_, 30), 0, state_.xer_so);
        if (state_.cr6.eq)
            break;
    }
    R(state_, 11) -= R(state_, 27);
    state_.xer_ca = std::uint8_t(SW(R(state_, 11)) < 0 &&
        (W(R(state_, 11)) & 1u) != 0);
    R(state_, 26) = static_cast<std::uint64_t>(SW(R(state_, 11)) >> 1);
}

void FormatEngine::FormatCountOutput()
{
    R(state_, 11) = WordRotateMask(R(state_, 24) + 7u, 0,
        0xfffffff8u);
    R(state_, 24) = R(state_, 11) + 8u;
    R(state_, 31) = memory_.ReadU32(Address(R(state_, 24) - 4u));
    CallSupport(0x82b85420u, memory_, dependencies_, state_, 0x8231986cu);
    CompareSigned(state_.cr0, R(state_, 3), 0, state_.xer_so);
    if (state_.cr0.eq)
    {
        FailInvalid();
        return;
    }
    R(state_, 11) = WordRotateMask(R(state_, 25), 0, 0x20u);
    CompareSigned(state_.cr0, R(state_, 11), 0, state_.xer_so);
    if (!state_.cr0.eq)
        memory_.WriteU16(Address(R(state_, 31)), H(R(state_, 22)));
    else
        memory_.WriteU32(Address(R(state_, 31)), W(R(state_, 22)));
    R(state_, 11) = 1;
    memory_.WriteU32(sp_ + 96u, 1);
}

void FormatEngine::ProcessSpecifier()
{
    R(state_, 11) = W(R(state_, 10) - 65u);
    CompareUnsigned(state_.cr6, R(state_, 11), 55u, state_.xer_so);
    if (state_.cr6.gt)
    {
        EmitField();
        return;
    }
    R(state_, 12) = 0xffffffff820d3660ull;
    R(state_, 0) = WordRotateMask(R(state_, 11), 1, 0xfffffffeu);
    R(state_, 0) = memory_.ReadU16(Address(R(state_, 12) + R(state_, 0)));
    R(state_, 12) = 0xffffffff8231968cull + R(state_, 0);
    state_.ctr = R(state_, 12);
    switch (Address(state_.ctr))
    {
    case 0x8231968cu: FormatCharacter(true); break;
    case 0x8231969cu: FormatCharacter(false); break;
    case 0x82319700u: FormatCountedString(); break;
    case 0x82319780u: FormatString(true); break;
    case 0x82319790u: FormatString(false); break;
    case 0x82319858u: FormatCountOutput(); return;
    case 0x82319894u: R(state_, 29) = H(R(state_, 10) + 32u);
        R(state_, 16) = 1; FormatFloat(); break;
    case 0x823198a0u: FormatFloat(); break;
    case 0x823199c8u: FormatInteger(0x823199c8u); break;
    case 0x823199ccu: FormatInteger(0x823199ccu); break;
    case 0x823199d4u: FormatInteger(0x823199d4u); break;
    case 0x823199d8u: FormatInteger(0x823199d8u); break;
    case 0x823199e0u: FormatInteger(0x823199e0u); break;
    case 0x82319a0cu: FormatInteger(0x82319a0cu); break;
    case 0x82319b74u: EmitField(); return;
    default: throw std::logic_error("unknown live formatter specifier target");
    }
    if (!aborted_)
        EmitField();
}

void FormatEngine::FormatFloat()
{
    R(state_, 25) |= 64u;
    R(state_, 27) = state_.sp + 128u;
    R(state_, 30) = 512;
    CompareSigned(state_.cr6, R(state_, 23), 0, state_.xer_so);
    if (state_.cr6.lt)
        R(state_, 23) = 6;
    else if (state_.cr6.eq && H(R(state_, 29)) == 'g')
        R(state_, 23) = 1;
    else
    {
        CompareSigned(state_.cr6, R(state_, 23), 512, state_.xer_so);
        if (state_.cr6.gt)
            R(state_, 23) = 512;
        CompareSigned(state_.cr6, R(state_, 23), 163, state_.xer_so);
        if (state_.cr6.gt)
        {
            R(state_, 31) = R(state_, 23) + 349u;
            R(state_, 3) = R(state_, 31);
            CallRawAllocation(memory_, dependencies_, state_);
            memory_.WriteU32(sp_ + 92u, W(R(state_, 3)));
            if (W(R(state_, 3)) != 0)
            {
                R(state_, 27) = R(state_, 3);
                R(state_, 30) = R(state_, 31);
            }
            else
                R(state_, 23) = 163;
        }
    }
    R(state_, 11) = R(state_, 24) + 7u;
    R(state_, 10) = memory_.ReadU32(Address(R(state_, 18)) + 24u);
    R(state_, 9) = R(state_, 20);
    R(state_, 11) = WordRotateMask(R(state_, 11), 0, 0xfffffff8u);
    R(state_, 8) = R(state_, 16);
    R(state_, 24) = R(state_, 11) + 8u;
    R(state_, 7) = R(state_, 23);
    R(state_, 6) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int8_t>(R(state_, 29))));
    R(state_, 5) = R(state_, 30);
    R(state_, 4) = R(state_, 27);
    R(state_, 11) = ReadU64(memory_, Address(R(state_, 24) - 8u));
    R(state_, 3) = state_.sp + 112u;
    WriteU64(memory_, sp_ + 112u, R(state_, 11));
    // The slot read above is live. bctrl writes its own return address.
    state_.ctr = R(state_, 10);
    state_.lr = 0x8231994cu;
    auto target = Address(state_.ctr) & ~GuestAddress{3};
    if (!crt_float_formatting::Apply(target, memory_,
            dependencies_.floating, state_) &&
        !crt_format_dispatch::Apply(target, memory_,
            dependencies_.output.streams.locks, state_))
        dependencies_.dynamic.CallGuestFormatter(target, memory_, state_);
    R(state_, 31) = WordRotateMask(R(state_, 25), 0, 0x80u);
    if (W(R(state_, 31)) != 0 && SW(R(state_, 23)) == 0)
    {
        R(state_, 4) = R(state_, 20);
        R(state_, 3) = R(state_, 27);
        CallFloatSlot(Address(R(state_, 18)) + 36u, memory_, dependencies_,
            state_, 0x82319970u, 11);
    }
    R(state_, 11) = H(R(state_, 29));
    CompareUnsigned(state_.cr6, R(state_, 11), 'g', state_.xer_so);
    if (state_.cr6.eq && W(R(state_, 31)) == 0)
    {
        R(state_, 4) = R(state_, 20);
        R(state_, 3) = R(state_, 27);
        CallFloatSlot(Address(R(state_, 18)) + 32u, memory_, dependencies_,
            state_, 0x82319998u, 11);
    }
    R(state_, 11) = memory_.ReadU8(Address(R(state_, 27)));
    CompareUnsigned(state_.cr6, R(state_, 11), '-', state_.xer_so);
    if (state_.cr6.eq)
    {
        R(state_, 25) |= 256u;
        R(state_, 27) += 1u;
    }
    R(state_, 11) = R(state_, 27);
    R(state_, 10) = R(state_, 11);
    do
    {
        R(state_, 9) = memory_.ReadU8(Address(R(state_, 11)));
        R(state_, 11) += 1u;
        CompareUnsigned(state_.cr6, R(state_, 9), 0, state_.xer_so);
    } while (!state_.cr6.eq);
    R(state_, 26) = W(R(state_, 11) - R(state_, 10) - 1u);
}

void FormatEngine::FormatInteger(GuestAddress branch)
{
    if (branch == 0x823199c8u)
    {
        R(state_, 25) |= 64u;
        branch = 0x823199ccu;
    }
    if (branch == 0x823199ccu)
        R(state_, 8) = 10;
    else if (branch == 0x823199d4u)
    {
        R(state_, 23) = 8;
        R(state_, 31) = 7;
        R(state_, 8) = 16;
    }
    else if (branch == 0x823199d8u || branch == 0x823199e0u)
    {
        R(state_, 31) = branch == 0x823199d8u ? 7u : 39u;
        memory_.WriteU32(sp_ + 104u, W(R(state_, 31)));
        R(state_, 8) = 16;
        if ((W(R(state_, 25)) & 128u) != 0)
        {
            memory_.WriteU16(sp_ + 90u, H(R(state_, 31) + 81u));
            memory_.WriteU16(sp_ + 88u, '0');
            R(state_, 17) = 2;
        }
    }
    else if (branch == 0x82319a0cu)
    {
        R(state_, 8) = 8;
        if ((W(R(state_, 25)) & 128u) != 0)
            R(state_, 25) |= 512u;
    }
    const auto flags = W(R(state_, 25));
    R(state_, 11) = WordRotateMask(R(state_, 24) + 7u, 0, 0xfffffff8u);
    R(state_, 24) = R(state_, 11) + 8u;
    if ((flags & (0x8000u | 0x1000u)) != 0)
        R(state_, 11) = ReadU64(memory_, Address(R(state_, 24) - 8u));
    else if ((flags & 0x20u) != 0)
    {
        R(state_, 11) = memory_.ReadU32(Address(R(state_, 24) - 4u));
        if ((flags & 0x40u) != 0)
            R(state_, 11) = static_cast<std::uint64_t>(
                static_cast<std::int64_t>(static_cast<std::int16_t>(R(state_, 11))));
        else
            R(state_, 11) = H(R(state_, 11));
    }
    else if ((flags & 0x40u) != 0)
        R(state_, 11) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int32_t>(
                memory_.ReadU32(Address(R(state_, 24) - 4u)))));
    else
        R(state_, 11) = memory_.ReadU32(Address(R(state_, 24) - 4u));
    if ((flags & 0x40u) != 0)
    {
        CompareSigned64(state_.cr6, R(state_, 11), 0, state_.xer_so);
        if (state_.cr6.lt)
        {
            R(state_, 11) = 0u - R(state_, 11);
            R(state_, 25) |= 256u;
        }
    }
    if ((flags & (0x8000u | 0x1000u)) == 0)
        R(state_, 11) = W(R(state_, 11));
    CompareSigned(state_.cr6, R(state_, 23), 0, state_.xer_so);
    if (state_.cr6.lt)
        R(state_, 23) = 1;
    else
    {
        R(state_, 25) &= ~8ull;
        if (SW(R(state_, 23)) > 512)
            R(state_, 23) = 512;
    }
    CompareUnsigned(state_.cr6, R(state_, 11), 0, state_.xer_so);
    if (R(state_, 11) == 0)
        R(state_, 17) = R(state_, 28);
    R(state_, 9) = state_.sp + 639u;
    for (;;)
    {
        CompareSigned(state_.cr6, R(state_, 23), 0, state_.xer_so);
        R(state_, 23) -= 1u;
        if (!state_.cr6.gt && R(state_, 11) == 0)
            break;
        const auto divisor = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(SW(R(state_, 8))));
        const auto quotient = R(state_, 11) / divisor;
        const auto digit = R(state_, 11) - quotient * divisor;
        R(state_, 7) = digit;
        R(state_, 11) = quotient;
        R(state_, 10) = W(digit) + '0';
        CompareSigned(state_.cr6, R(state_, 10), '9', state_.xer_so);
        if (state_.cr6.gt)
            R(state_, 10) += R(state_, 31);
        memory_.WriteU8(Address(R(state_, 9)), static_cast<std::uint8_t>(R(state_, 10)));
        R(state_, 9) -= 1u;
    }
    R(state_, 11) = state_.sp + 639u;
    R(state_, 26) = W(R(state_, 11) - R(state_, 9));
    R(state_, 27) = R(state_, 9) + 1u;
    if ((W(R(state_, 25)) & 512u) != 0 &&
        (W(R(state_, 26)) == 0 ||
            memory_.ReadU8(Address(R(state_, 27))) != '0'))
    {
        R(state_, 27) -= 1u;
        R(state_, 26) += 1u;
        memory_.WriteU8(Address(R(state_, 27)), '0');
    }
}

void FormatEngine::EmitField()
{
    R(state_, 11) = memory_.ReadU32(sp_ + 96u);
    CompareSigned(state_.cr6, R(state_, 11), 0, state_.xer_so);
    if (!state_.cr6.eq)
        return;
    if ((W(R(state_, 25)) & 64u) != 0)
    {
        if ((W(R(state_, 25)) & 256u) != 0)
            R(state_, 11) = '-';
        else if ((W(R(state_, 25)) & 1u) != 0)
            R(state_, 11) = '+';
        else if ((W(R(state_, 25)) & 2u) != 0)
            R(state_, 11) = ' ';
        else
            R(state_, 11) = 0;
        if (W(R(state_, 11)) != 0)
        {
            memory_.WriteU16(sp_ + 88u, H(R(state_, 11)));
            R(state_, 17) = 1;
        }
    }
    R(state_, 11) = R(state_, 14) - R(state_, 26);
    R(state_, 28) = R(state_, 11) - R(state_, 17);
    if ((W(R(state_, 25)) & 12u) == 0 && SW(R(state_, 28)) > 0)
    {
        R(state_, 31) = R(state_, 28);
        while (SW(R(state_, 31)) > 0)
        {
            R(state_, 31) -= 1u;
            EmitCodeUnit(' ', 0x82319becu);
            if (SW(R(state_, 22)) == -1)
                break;
        }
    }
    R(state_, 11) = memory_.ReadU32(Address(R(state_, 21)) + 12u);
    R(state_, 30) = R(state_, 17);
    R(state_, 31) = state_.sp + 88u;
    if ((W(R(state_, 11)) & 64u) != 0 &&
        memory_.ReadU32(Address(R(state_, 21)) + 8u) == 0)
    {
        R(state_, 22) = W(R(state_, 22) + R(state_, 17));
        memory_.WriteU32(sp_ + 80u, W(R(state_, 22)));
    }
    else
    {
        while (SW(R(state_, 30)) > 0)
        {
            R(state_, 3) = memory_.ReadU16(Address(R(state_, 31)));
            R(state_, 30) -= 1u;
            R(state_, 5) = state_.sp + 80u;
            R(state_, 4) = R(state_, 21);
            CallOutput(0x82b84a08u, memory_, dependencies_, state_,
                0x82319c48u);
            R(state_, 22) = memory_.ReadU32(sp_ + 80u);
            R(state_, 31) += 2u;
            if (SW(R(state_, 22)) == -1)
            {
                RecoverError(0x82319c5cu);
                R(state_, 11) = memory_.ReadU32(Address(R(state_, 3)));
                CompareSigned(state_.cr6, R(state_, 11), 42, state_.xer_so);
                if (state_.cr6.eq)
                    EmitCodeUnit('?', 0x82319c78u);
                else
                    break;
            }
        }
    }
    if ((W(R(state_, 25)) & 8u) != 0 &&
        (W(R(state_, 25)) & 4u) == 0 && SW(R(state_, 28)) > 0)
    {
        R(state_, 31) = R(state_, 28);
        while (SW(R(state_, 31)) > 0)
        {
            R(state_, 31) -= 1u;
            EmitCodeUnit('0', 0x82319cb4u);
            if (SW(R(state_, 22)) == -1)
                break;
        }
    }
    if (SW(R(state_, 19)) == 0 && SW(R(state_, 26)) > 0)
    {
        R(state_, 30) = R(state_, 27);
        R(state_, 31) = R(state_, 26);
        while (SW(R(state_, 31)) > 0)
        {
            R(state_, 11) = memory_.ReadU32(Address(R(state_, 20)));
            R(state_, 4) = R(state_, 30);
            R(state_, 6) = R(state_, 20);
            R(state_, 3) = state_.sp + 84u;
            R(state_, 31) -= 1u;
            R(state_, 5) = memory_.ReadU32(Address(R(state_, 11)) + 172u);
            CallSupport(0x822a07a0u, memory_, dependencies_, state_,
                0x82319cfcu);
            R(state_, 29) = R(state_, 3);
            CompareSigned(state_.cr0, R(state_, 29), 0, state_.xer_so);
            if (!state_.cr0.gt)
            {
                R(state_, 22) = ~std::uint64_t{0};
                memory_.WriteU32(sp_ + 80u, 0xffffffffu);
                break;
            }
            EmitCodeUnit(memory_.ReadU16(sp_ + 84u), 0x82319d14u);
            R(state_, 30) += R(state_, 29);
        }
    }
    else
    {
        R(state_, 30) = R(state_, 26);
        R(state_, 31) = R(state_, 27);
        R(state_, 11) = memory_.ReadU32(Address(R(state_, 21)) + 12u);
        if ((W(R(state_, 11)) & 64u) != 0 &&
            memory_.ReadU32(Address(R(state_, 21)) + 8u) == 0)
        {
            R(state_, 22) = W(R(state_, 22) + R(state_, 26));
            memory_.WriteU32(sp_ + 80u, W(R(state_, 22)));
        }
        else
        {
            while (SW(R(state_, 30)) > 0)
            {
                R(state_, 3) = memory_.ReadU16(Address(R(state_, 31)));
                R(state_, 30) -= 1u;
                R(state_, 5) = state_.sp + 80u;
                R(state_, 4) = R(state_, 21);
                CallOutput(0x82b84a08u, memory_, dependencies_, state_,
                    0x82319d78u);
                R(state_, 22) = memory_.ReadU32(sp_ + 80u);
                R(state_, 31) += 2u;
                if (SW(R(state_, 22)) == -1)
                {
                    RecoverError(0x82319d8cu);
                    R(state_, 11) = memory_.ReadU32(Address(R(state_, 3)));
                    CompareSigned(state_.cr6, R(state_, 11), 42,
                        state_.xer_so);
                    if (!state_.cr6.eq)
                        break;
                    EmitCodeUnit('?', 0x82319da8u);
                }
            }
        }
    }
    if (SW(R(state_, 22)) >= 0 && (W(R(state_, 25)) & 4u) != 0 &&
        SW(R(state_, 28)) > 0)
    {
        R(state_, 31) = R(state_, 28);
        while (SW(R(state_, 31)) > 0)
        {
            R(state_, 31) -= 1u;
            EmitCodeUnit(' ', 0x82319de4u);
            if (SW(R(state_, 22)) == -1)
                break;
        }
    }
}

void FormatEngine::Run()
{
    SaveFrame(memory_, state_, 14, 2336);
    sp_ = Address(state_.sp);
    R(state_, 28) = 0;
    R(state_, 21) = R(state_, 3);
    R(state_, 31) = 0;
    R(state_, 15) = R(state_, 4);
    R(state_, 24) = R(state_, 6);
    R(state_, 25) = 0;
    memory_.WriteU32(sp_ + 96u, 0);
    R(state_, 14) = 0;
    memory_.WriteU32(sp_ + 104u, 0);
    R(state_, 23) = 0;
    R(state_, 17) = 0;
    R(state_, 19) = 0;
    CompareUnsigned(state_.cr6, R(state_, 21), 0, state_.xer_so);
    if (state_.cr6.eq)
        FailInvalid();
    else
    {
        CompareUnsigned(state_.cr6, R(state_, 15), 0, state_.xer_so);
        if (state_.cr6.eq)
            FailInvalid();
    }
    if (aborted_)
    {
        RestoreFrame(memory_, state_, 14, 2336);
        return;
    }
    R(state_, 3) = R(state_, 21);
    CallOutput(0x822a03c8u, memory_, dependencies_, state_, 0x823193b0u);
    R(state_, 11) = R(state_, 3) + 32u;
    CompareUnsigned(state_.cr6, R(state_, 21), R(state_, 11), state_.xer_so);
    bool reserved = state_.cr6.eq;
    if (!reserved)
    {
        R(state_, 3) = R(state_, 21);
        CallOutput(0x822a03c8u, memory_, dependencies_, state_,
            0x823193c0u);
        R(state_, 11) = R(state_, 3) + 64u;
        CompareUnsigned(state_.cr6, R(state_, 21), R(state_, 11),
            state_.xer_so);
        reserved = state_.cr6.eq;
    }
    if (reserved)
    {
        R(state_, 6) = R(state_, 24);
        R(state_, 5) = R(state_, 15);
        R(state_, 4) = 512;
        R(state_, 3) = state_.sp + 1152u;
        state_.lr = 0x82319e50u;
        FormatEntry(memory_, dependencies_, state_);
        R(state_, 31) = R(state_, 3);
        CompareSigned(state_.cr6, R(state_, 31), -1, state_.xer_so);
        if (state_.cr6.eq)
        {
            R(state_, 31) = 511;
            memory_.WriteU16(sp_ + 2174u, 0);
        }
        R(state_, 3) = state_.sp + 1152u;
        CallOutput(0x82be4700u, memory_, dependencies_, state_,
            0x82319e6cu);
        R(state_, 3) = R(state_, 31);
        RestoreFrame(memory_, state_, 14, 2336);
        return;
    }
    R(state_, 29) = memory_.ReadU16(Address(R(state_, 15)));
    R(state_, 22) = 0;
    R(state_, 26) = 0;
    memory_.WriteU32(sp_ + 92u, 0);
    R(state_, 8) = 0;
    memory_.WriteU32(sp_ + 80u, 0);
    R(state_, 10) = R(state_, 29);
    if (W(R(state_, 10)) != 0)
    {
        R(state_, 20) = 0xffffffff83215300ull;
        R(state_, 18) = 0xffffffff83214fc8ull;
        R(state_, 9) = 0xffffffff820d37e0ull;
        memory_.WriteU32(sp_ + 100u, W(R(state_, 9)));
        R(state_, 16) = memory_.ReadU32(sp_ + 100u);
        R(state_, 27) = R(state_, 16);
        while (W(R(state_, 10)) != 0 && SW(R(state_, 22)) >= 0)
        {
            ProcessToken();
            if (aborted_)
                break;
            R(state_, 11) = memory_.ReadU32(sp_ + 92u);
            CompareUnsigned(state_.cr6, R(state_, 11), 0, state_.xer_so);
            if (!state_.cr6.eq)
            {
                R(state_, 3) = R(state_, 11);
                CallFree(memory_, dependencies_, state_);
                R(state_, 11) = 0;
                memory_.WriteU32(sp_ + 92u, 0);
            }
            NextCodeUnit();
        }
    }
    R(state_, 3) = aborted_ ? ~std::uint64_t{0} : R(state_, 22);
    RestoreFrame(memory_, state_, 14, 2336);
}

void FormatStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    FormatEngine(memory, dependencies, state).Run();
}

void FormatBuffer(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void FormatUnbounded(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void FormatStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state);
void FormatEntry(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 7) = R(state, 6);
    R(state, 6) = 0;
    FormatBuffer(memory, dependencies, state);
}

void FormatBuffer(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveFrame(memory, state, 30, 144);
    const auto sp = Address(state.sp);
    R(state, 11) = R(state, 5);
    R(state, 31) = R(state, 3);
    R(state, 5) = R(state, 6);
    CompareUnsigned(state.cr6, R(state, 11), 0, state.xer_so);
    if (state.cr6.eq ||
        (W(R(state, 4)) != 0 && W(R(state, 31)) == 0))
    {
        if (!state.cr6.eq)
        {
            CompareUnsigned(state.cr6, R(state, 4), 0, state.xer_so);
            CompareUnsigned(state.cr6, R(state, 31), 0, state.xer_so);
        }
        CallOutput(0x82b7fd78u, memory, dependencies, state, 0x82b7d04cu);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22;
        for (unsigned index = 3; index <= 7; ++index)
            R(state, index) = 0;
        memory.WriteU32(Address(R(state, 11)), 22);
        CallOutput(0x82b7fec0u, memory, dependencies, state, 0x82b7d070u);
        R(state, 3) = ~std::uint64_t{0};
        RestoreFrame(memory, state, 30, 144);
        return;
    }
    CompareUnsigned(state.cr6, R(state, 4), 0, state.xer_so);
    if (!state.cr6.eq)
        CompareUnsigned(state.cr6, R(state, 31), 0, state.xer_so);
    R(state, 10) = 0x3fff0000u;
    memory.WriteU32(sp + 88u, W(R(state, 31)));
    R(state, 9) = 66;
    memory.WriteU32(sp + 80u, W(R(state, 31)));
    R(state, 10) |= 0xffffu;
    CompareUnsigned(state.cr6, R(state, 4), R(state, 10), state.xer_so);
    memory.WriteU32(sp + 92u, 66);
    if (state.cr6.gt)
        R(state, 10) = 0x7fffffffu;
    else
        R(state, 10) = WordRotateMask(R(state, 4), 1, 0xfffffffeu);
    R(state, 6) = R(state, 7);
    memory.WriteU32(sp + 84u, W(R(state, 10)));
    R(state, 4) = R(state, 11);
    R(state, 3) = state.sp + 80u;
    state.lr = 0x82b7d0ccu;
    FormatStream(memory, dependencies, state);
    R(state, 30) = R(state, 3);
    CompareUnsigned(state.cr6, R(state, 31), 0, state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 31) = 0;
        for (unsigned attempt = 0; attempt < 2; ++attempt)
        {
            R(state, 11) = memory.ReadU32(sp + 84u);
            AddImmediate(state, 11, -1);
            memory.WriteU32(sp + 84u, W(R(state, 11)));
            if (!state.cr0.lt)
            {
                R(state, 11) = memory.ReadU32(sp + 80u);
                memory.WriteU8(Address(R(state, 11)), 0);
                if (attempt == 0)
                {
                    R(state, 11) = memory.ReadU32(sp + 80u);
                    R(state, 11) += 1u;
                    memory.WriteU32(sp + 80u, W(R(state, 11)));
                }
            }
            else
            {
                R(state, 4) = state.sp + 80u;
                R(state, 3) = 0;
                CallOutput(0x82b82558u, memory, dependencies, state,
                    attempt == 0 ? 0x82b7d110u : 0x82b7d138u);
            }
        }
    }
    R(state, 3) = R(state, 30);
    RestoreFrame(memory, state, 30, 144);
}

void FormatUnbounded(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveFrame(memory, state, 30, 144);
    const auto sp = Address(state.sp);
    R(state, 11) = R(state, 3);
    CompareUnsigned(state.cr6, R(state, 4), 0, state.xer_so);
    if (state.cr6.eq)
    {
        CallOutput(0x82b7fd78u, memory, dependencies, state, 0x82b7ca34u);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22;
        for (unsigned index = 3; index <= 7; ++index)
            R(state, index) = 0;
        memory.WriteU32(Address(R(state, 11)), 22);
        CallOutput(0x82b7fec0u, memory, dependencies, state, 0x82b7ca58u);
        R(state, 3) = ~std::uint64_t{0};
        RestoreFrame(memory, state, 30, 144);
        return;
    }
    CompareUnsigned(state.cr6, R(state, 11), 0, state.xer_so);
    if (state.cr6.eq)
    {
        CallOutput(0x82b7fd78u, memory, dependencies, state, 0x82b7ca34u);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22;
        for (unsigned index = 3; index <= 7; ++index)
            R(state, index) = 0;
        memory.WriteU32(Address(R(state, 11)), 22);
        CallOutput(0x82b7fec0u, memory, dependencies, state, 0x82b7ca58u);
        R(state, 3) = ~std::uint64_t{0};
        RestoreFrame(memory, state, 30, 144);
        return;
    }
    memory.WriteU32(sp + 88u, W(R(state, 11)));
    R(state, 10) = 66;
    memory.WriteU32(sp + 80u, W(R(state, 11)));
    R(state, 11) = 0x7fffffffu;
    R(state, 3) = state.sp + 80u;
    memory.WriteU32(sp + 92u, 66u);
    memory.WriteU32(sp + 84u, W(R(state, 11)));
    state.lr = 0x82b7ca8cu;
    FormatStream(memory, dependencies, state);
    R(state, 30) = R(state, 3);
    R(state, 31) = 0;
    R(state, 11) = memory.ReadU32(sp + 84u);
    AddImmediate(state, 11, -1);
    memory.WriteU32(sp + 84u, W(R(state, 11)));
    if (!state.cr0.lt)
    {
        R(state, 11) = memory.ReadU32(sp + 80u);
        memory.WriteU8(Address(R(state, 11)), 0);
        R(state, 11) += 1u;
        memory.WriteU32(sp + 80u, W(R(state, 11)));
    }
    else
    {
        R(state, 4) = state.sp + 80u;
        R(state, 3) = 0;
        CallOutput(0x82b82558u, memory, dependencies, state, 0x82b7cac0u);
    }
    R(state, 11) = memory.ReadU32(sp + 84u);
    AddImmediate(state, 11, -1);
    memory.WriteU32(sp + 84u, W(R(state, 11)));
    if (!state.cr0.lt)
    {
        R(state, 11) = memory.ReadU32(sp + 80u);
        memory.WriteU8(Address(R(state, 11)), 0);
    }
    else
    {
        R(state, 4) = state.sp + 80u;
        R(state, 3) = 0;
        CallOutput(0x82b82558u, memory, dependencies, state, 0x82b7caecu);
    }
    R(state, 3) = R(state, 30);
    RestoreFrame(memory, state, 30, 144);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (entry)
    {
    case 0x82b7d158u: FormatEntry(memory, dependencies, registers); return true;
    case 0x82b7d020u: FormatBuffer(memory, dependencies, registers); return true;
    case 0x82319330u: FormatStream(memory, dependencies, registers); return true;
    case 0x82b7ca10u: FormatUnbounded(memory, dependencies, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_formatter
