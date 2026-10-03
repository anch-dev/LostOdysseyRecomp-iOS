#include "lo_semantics/crt_float_core_helpers.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_float_core_helpers
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
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

// Convert raw binary64 bits into the ten-byte extended representation. Read
// the source in the PPC order before writing the destination: they may alias.
void ExpandBinary64(GuestMemory& memory, Registers& state)
{
    const auto input = Address(R(state, 4));
    const auto output = Address(R(state, 3));
    R(state, 11) = memory.ReadU16(input);
    R(state, 10) = 0xffffffff80000000ull;
    R(state, 8) = memory.ReadU32(input);
    R(state, 6) = WordRotateMask(R(state, 11), 0, 0xffff8000u);
    R(state, 9) = memory.ReadU32(input + 4u);
    R(state, 11) = WordRotateMask(R(state, 11), 28, 0x7ffu);
    R(state, 7) = static_cast<std::uint32_t>(R(state, 8)) & 0xfffffu;
    R(state, 11) = static_cast<std::uint32_t>(R(state, 11)) & 0xffffu;
    CompareSigned(state.cr0, R(state, 11), 0, state.xer_so);

    if (state.cr0.eq)
    {
        CompareUnsigned(state.cr6, R(state, 7), 0, state.xer_so);
        if (state.cr6.eq)
            CompareUnsigned(state.cr6, R(state, 9), 0, state.xer_so);
        if (state.cr6.eq)
        {
            R(state, 11) = 0;
            memory.WriteU16(output, static_cast<std::uint16_t>(R(state, 6)));
            memory.WriteU32(output + 2u, 0);
            memory.WriteU32(output + 6u, 0);
            return;
        }
        R(state, 11) += 15361u;
        R(state, 10) = 0;
        R(state, 8) = static_cast<std::uint32_t>(R(state, 11)) & 0xffffu;
    }
    else
    {
        CompareSigned(state.cr6, R(state, 11), 2047, state.xer_so);
        if (state.cr6.eq)
            R(state, 8) = 32767;
        else
        {
            R(state, 11) += 15360u;
            R(state, 8) = static_cast<std::uint32_t>(R(state, 11)) & 0xffffu;
        }
    }

    R(state, 11) = WordRotateMask(R(state, 9), 11, 0x7ffu);
    R(state, 7) = WordRotateMask(R(state, 7), 11, 0xfffff800u);
    R(state, 9) = WordRotateMask(R(state, 9), 11, 0xfffff800u);
    R(state, 11) |= R(state, 7);
    R(state, 11) |= R(state, 10);
    memory.WriteU32(output + 6u, Address(R(state, 9)));
    memory.WriteU32(output + 2u, Address(R(state, 11)));
    R(state, 10) = WordRotateMask(R(state, 11), 0, 0x80000000u);
    CompareSigned(state.cr0, R(state, 10), 0, state.xer_so);

    while (state.cr0.eq)
    {
        // Subnormals normalize through live destination words. Keep each
        // load/store and the exponent decrement in original instruction order.
        R(state, 10) = static_cast<std::uint32_t>(R(state, 8)) & 0xffffu;
        R(state, 11) = memory.ReadU32(output + 6u);
        R(state, 9) = memory.ReadU32(output + 2u);
        R(state, 10) += 65536u;
        R(state, 9) = WordRotateMask(R(state, 9), 1, 0xfffffffeu);
        R(state, 10) -= 1u;
        R(state, 8) = static_cast<std::uint32_t>(R(state, 10)) & 0xffffu;
        R(state, 10) = WordRotateMask(R(state, 11), 1, 1u);
        R(state, 11) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
        R(state, 10) |= R(state, 9);
        memory.WriteU32(output + 6u, Address(R(state, 11)));
        R(state, 11) = static_cast<std::uint32_t>(R(state, 10));
        memory.WriteU32(output + 2u, Address(R(state, 10)));
        R(state, 11) = WordRotateMask(R(state, 11), 0, 0x80000000u);
        CompareSigned(state.cr0, R(state, 11), 0, state.xer_so);
    }

    R(state, 11) = static_cast<std::uint32_t>(R(state, 6)) & 0xffffu;
    R(state, 10) = static_cast<std::uint32_t>(R(state, 8)) & 0xffffu;
    R(state, 11) |= R(state, 10);
    memory.WriteU16(output, static_cast<std::uint16_t>(R(state, 11)));
}

void EnterCopyFrame(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(state.sp - 16u), R(state, 31));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
}

void LeaveCopyFrame(GuestMemory& memory, Registers& state)
{
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

class ErrorAddressServices final : public AllocationFailureServices
{
public:
    ErrorAddressServices(GuestMemory& memory, CrtThreadDataServices& thread,
        Registers& state) : memory_(memory), thread_(thread), state_(state) {}

    std::uint64_t GetThreadData() override
    {
        CrtThreadDataCall call{R(state_, 13)};
        const auto record = GetCrtThreadData(memory_, thread_, call);
        R(state_, 13) = call.thread_environment;
        R(state_, 3) = record;
        return record;
    }
    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::logic_error("unexpected CRT error output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::logic_error("unexpected CRT bug check"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::logic_error("unexpected CRT new handler"); }

private:
    GuestMemory& memory_;
    CrtThreadDataServices& thread_;
    Registers& state_;
};

void CallErrorAddress(GuestMemory& memory, Dependencies dependencies,
    Registers& state, std::uint32_t return_address)
{
    state.lr = return_address;
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u; // 82B7FD78 owns this nested frame.
    ErrorAddressServices services(memory, dependencies.thread, state);
    R(state, 3) = GetAllocationErrorAddress(services);
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void CallInvalid(GuestMemory& memory, Dependencies dependencies,
    Registers& state, std::uint32_t return_address)
{
    state.lr = return_address;
    InvalidParameterCall call{{{R(state, 3), R(state, 4), R(state, 5),
        R(state, 6), R(state, 7), R(state, 8), R(state, 9), R(state, 10)}},
        R(state, 13)};
    R(state, 3) = ReportInvalidParameter(memory, dependencies.invalid, call);
    for (unsigned index = 1; index < 8; ++index)
        R(state, index + 3u) = call.arguments[index];
    R(state, 13) = call.thread_environment;
}

void ReportCopyError(GuestMemory& memory, Dependencies dependencies,
    Registers& state, std::uint32_t code, std::uint32_t getter_return)
{
    CallErrorAddress(memory, dependencies, state, getter_return);
    R(state, 31) = code;
    memory.WriteU32(Address(R(state, 3)), Address(R(state, 31)));
    R(state, 7) = 0;
    R(state, 6) = 0;
    R(state, 5) = 0;
    R(state, 4) = 0;
    R(state, 3) = 0;
    CallInvalid(memory, dependencies, state, 0x8231b154u);
    R(state, 3) = R(state, 31);
}

void CopyBounded(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    EnterCopyFrame(memory, state);
    CompareUnsigned(state.cr6, R(state, 3), 0, state.xer_so);
    if (!state.cr6.eq)
        CompareUnsigned(state.cr6, R(state, 4), 0, state.xer_so);
    if (state.cr6.eq)
    {
        CallErrorAddress(memory, dependencies, state, 0x8231b0f4u);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22;
        R(state, 7) = 0;
        R(state, 6) = 0;
        R(state, 5) = 0;
        R(state, 4) = 0;
        R(state, 3) = 0;
        memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
        CallInvalid(memory, dependencies, state, 0x8231b118u);
        R(state, 3) = 22;
        LeaveCopyFrame(memory, state);
        return;
    }

    CompareUnsigned(state.cr6, R(state, 5), 0, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 11) = 0;
        memory.WriteU8(Address(R(state, 3)), 0);
        ReportCopyError(memory, dependencies, state, 22, 0x8231b134u);
        LeaveCopyFrame(memory, state);
        return;
    }

    R(state, 11) = R(state, 3);
    for (;;)
    {
        R(state, 10) = memory.ReadU8(Address(R(state, 5)));
        R(state, 5) += 1u;
        CompareUnsigned(state.cr0, R(state, 10), 0, state.xer_so);
        memory.WriteU8(Address(R(state, 11)),
            static_cast<std::uint8_t>(R(state, 10)));
        R(state, 11) += 1u;
        if (state.cr0.eq)
            break;
        state.xer_ca = std::uint8_t(static_cast<std::uint32_t>(R(state, 4)) > 0);
        R(state, 4) -= 1u;
        CompareSigned(state.cr0, R(state, 4), 0, state.xer_so);
        if (state.cr0.eq)
            break;
    }

    CompareUnsigned(state.cr6, R(state, 4), 0, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 11) = 0;
        memory.WriteU8(Address(R(state, 3)), 0);
        ReportCopyError(memory, dependencies, state, 34, 0x8231b194u);
    }
    else
        R(state, 3) = 0;
    LeaveCopyFrame(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (entry)
    {
    case 0x8231a390u: ExpandBinary64(memory, registers); return true;
    case 0x8231b0d0u: CopyBounded(memory, dependencies, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_float_core_helpers
