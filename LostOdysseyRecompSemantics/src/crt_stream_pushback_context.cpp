#include "lo_semantics/crt_stream_pushback_context.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_pushback_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t W(std::uint64_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t S(std::uint64_t value) { return std::bit_cast<std::int32_t>(W(value)); }

template<class T>
void Compare(crt_async_status_transfer::Condition& condition, T left,
    T right, std::uint8_t so)
{
    condition = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), so};
}

void Push(GuestMemory& memory, Registers& state, unsigned bytes)
{
    const auto old_sp = R(state, 1);
    memory.WriteU32(Address(old_sp - bytes), Address(old_sp));
    R(state, 1) = old_sp - bytes;
}

void Save27(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 8u * (33u - index)), R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), W(R(state, 12)));
}

void Restore27(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 27u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory, Address(R(state, 1) - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

void Direct(GuestAddress entry, GuestMemory& memory, Dependencies deps,
    Registers& state)
{
    if (!crt_reallocation_context::ApplyLower(entry, memory,
            deps.reallocation, state))
        throw std::logic_error("unselected CRT pushback direct guest callee");
}

void ReadStreamField(GuestMemory& memory, Dependencies deps, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), W(R(state, 12)));
    Push(memory, state, 96u);
    Compare<std::uint32_t>(state.cr6, W(R(state, 3)), 0u, state.xer_so);
    if (!state.cr6.eq)
        R(state, 3) = memory.ReadU32(Address(R(state, 3) + 16u));
    else
    {
        state.lr = 0x82b81660u;
        Direct(0x82b7fd78u, memory, deps, state);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22u;
        for (unsigned index = 3u; index <= 7u; ++index) R(state, index) = 0;
        memory.WriteU32(Address(R(state, 11)), W(R(state, 10)));
        state.lr = 0x82b81684u;
        Direct(0x82b7fec0u, memory, deps, state);
        R(state, 3) = std::uint64_t(std::int64_t(-1));
    }
    R(state, 1) += 96u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

void InitializeBuffer(GuestMemory& memory, Dependencies deps, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), W(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    Push(memory, state, 96u);
    R(state, 11) = std::uint64_t(std::int64_t(-2094202880));
    R(state, 31) = R(state, 3);
    R(state, 3) = 4096u;
    R(state, 10) = memory.ReadU32(Address(R(state, 11) + 15032u));
    R(state, 10) += 1u;
    memory.WriteU32(Address(R(state, 11) + 15032u), W(R(state, 10)));
    state.lr = 0x82b85c6cu;
    Direct(0x823acbd0u, memory, deps, state);
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 12u));
    Compare<std::uint32_t>(state.cr0, W(R(state, 3)), 0u, state.xer_so);
    memory.WriteU32(Address(R(state, 31) + 8u), W(R(state, 3)));
    if (!state.cr0.eq)
    {
        R(state, 10) = 4096u;
        R(state, 11) |= 8u;
        memory.WriteU32(Address(R(state, 31) + 24u), W(R(state, 10)));
    }
    else
    {
        R(state, 10) = R(state, 31) + 20u;
        R(state, 9) = 2u;
        R(state, 11) |= 4u;
        memory.WriteU32(Address(R(state, 31) + 8u), W(R(state, 10)));
        memory.WriteU32(Address(R(state, 31) + 24u), W(R(state, 9)));
    }
    memory.WriteU32(Address(R(state, 31) + 12u), W(R(state, 11)));
    R(state, 10) = 0;
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 8u));
    memory.WriteU32(Address(R(state, 31) + 4u), W(R(state, 10)));
    memory.WriteU32(Address(R(state, 31)), W(R(state, 11)));
    R(state, 1) += 96u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 16u));
}

void Pushback(GuestMemory& memory, Dependencies deps, Registers& state)
{
    R(state, 12) = state.lr;
    state.lr = 0x82b87d00u;
    Save27(memory, state);
    Push(memory, state, 128u);
    R(state, 31) = R(state, 4);
    R(state, 27) = R(state, 3);
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 12u));
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x40u);
    Compare<std::int32_t>(state.cr0, S(R(state, 11)), 0, state.xer_so);
    if (!state.cr0.eq) goto valid_stream;
    R(state, 3) = R(state, 31);
    state.lr = 0x82b87d20u;
    ReadStreamField(memory, deps, state);
    R(state, 11) = std::uint64_t(std::int64_t(-2093481984));
    Compare<std::int32_t>(state.cr6, S(R(state, 3)), -1, state.xer_so);
    R(state, 29) = R(state, 11) - 29312u;
    R(state, 11) = std::uint64_t(std::int64_t(-2094989312));
    R(state, 28) = R(state, 11) + 21272u;
    if (state.cr6.eq) goto first_default;
    R(state, 3) = R(state, 31);
    state.lr = 0x82b87d40u;
    ReadStreamField(memory, deps, state);
    Compare<std::int32_t>(state.cr6, S(R(state, 3)), -2, state.xer_so);
    if (state.cr6.eq) goto first_default;
    R(state, 3) = R(state, 31);
    state.lr = 0x82b87d50u;
    ReadStreamField(memory, deps, state);
    state.xer_ca = std::uint8_t(S(R(state, 3)) < 0 && (W(R(state, 3)) & 31u) != 0u);
    R(state, 11) = std::uint64_t(std::int64_t(S(R(state, 3)) >> 5));
    R(state, 3) = R(state, 31);
    R(state, 30) = WordRotateMask(R(state, 11), 2, 0xfffffffcu);
    state.lr = 0x82b87d60u;
    ReadStreamField(memory, deps, state);
    R(state, 10) = memory.ReadU32(Address(R(state, 30) + R(state, 29)));
    R(state, 11) = WordRotateMask(R(state, 3), 6, 0x7c0u);
    R(state, 11) += R(state, 10);
    goto first_table;
first_default:
    R(state, 11) = R(state, 28);
first_table:
    R(state, 11) = memory.ReadU8(Address(R(state, 11) + 40u));
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0xfffffffeu);
    Compare<std::int32_t>(state.cr0, S(R(state, 11)), 0, state.xer_so);
    if (!state.cr0.eq) goto invalid;
    R(state, 3) = R(state, 31);
    state.lr = 0x82b87d88u;
    ReadStreamField(memory, deps, state);
    Compare<std::int32_t>(state.cr6, S(R(state, 3)), -1, state.xer_so);
    if (state.cr6.eq) goto second_default;
    R(state, 3) = R(state, 31);
    state.lr = 0x82b87d98u;
    ReadStreamField(memory, deps, state);
    Compare<std::int32_t>(state.cr6, S(R(state, 3)), -2, state.xer_so);
    if (state.cr6.eq) goto second_default;
    R(state, 3) = R(state, 31);
    state.lr = 0x82b87da8u;
    ReadStreamField(memory, deps, state);
    state.xer_ca = std::uint8_t(S(R(state, 3)) < 0 && (W(R(state, 3)) & 31u) != 0u);
    R(state, 11) = std::uint64_t(std::int64_t(S(R(state, 3)) >> 5));
    R(state, 3) = R(state, 31);
    R(state, 30) = WordRotateMask(R(state, 11), 2, 0xfffffffcu);
    state.lr = 0x82b87db8u;
    ReadStreamField(memory, deps, state);
    R(state, 10) = memory.ReadU32(Address(R(state, 30) + R(state, 29)));
    R(state, 11) = WordRotateMask(R(state, 3), 6, 0x7c0u);
    R(state, 11) += R(state, 10);
    goto second_table;
second_default:
    R(state, 11) = R(state, 28);
second_table:
    R(state, 11) = memory.ReadU8(Address(R(state, 11) + 40u));
    R(state, 11) = W(R(state, 11)) & 1u;
    Compare<std::int32_t>(state.cr0, S(R(state, 11)), 0, state.xer_so);
    if (state.cr0.eq) goto valid_stream;
invalid:
    state.lr = 0x82b87ddcu;
    Direct(0x82b7fd78u, memory, deps, state);
    R(state, 11) = R(state, 3);
    R(state, 10) = 22u;
    for (unsigned index = 3u; index <= 7u; ++index) R(state, index) = 0;
    memory.WriteU32(Address(R(state, 11)), W(R(state, 10)));
    state.lr = 0x82b87e00u;
    Direct(0x82b7fec0u, memory, deps, state);
error:
    R(state, 3) = std::uint64_t(std::int64_t(-1));
done:
    R(state, 1) += 128u;
    Restore27(memory, state);
    return;
valid_stream:
    Compare<std::int32_t>(state.cr6, S(R(state, 27)), -1, state.xer_so);
    if (state.cr6.eq) goto error;
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 12u));
    R(state, 10) = W(R(state, 11)) & 1u;
    Compare<std::int32_t>(state.cr0, S(R(state, 10)), 0, state.xer_so);
    if (!state.cr0.eq) goto buffer;
    R(state, 10) = WordRotateMask(R(state, 11), 0, 0x80u);
    Compare<std::int32_t>(state.cr0, S(R(state, 10)), 0, state.xer_so);
    if (state.cr0.eq) goto error;
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x2u);
    Compare<std::int32_t>(state.cr0, S(R(state, 11)), 0, state.xer_so);
    if (!state.cr0.eq) goto error;
buffer:
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 8u));
    Compare<std::uint32_t>(state.cr6, W(R(state, 11)), 0u, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 3) = R(state, 31);
        state.lr = 0x82b87e44u;
        InitializeBuffer(memory, deps, state);
    }
    R(state, 11) = memory.ReadU32(Address(R(state, 31)));
    R(state, 10) = memory.ReadU32(Address(R(state, 31) + 8u));
    Compare<std::uint32_t>(state.cr6, W(R(state, 11)), W(R(state, 10)), state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 10) = memory.ReadU32(Address(R(state, 31) + 4u));
        Compare<std::int32_t>(state.cr6, S(R(state, 10)), 0, state.xer_so);
        if (!state.cr6.eq) goto error;
        R(state, 11) += 1u;
        memory.WriteU32(Address(R(state, 31)), W(R(state, 11)));
    }
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 12u));
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x40u);
    Compare<std::int32_t>(state.cr0, S(R(state, 11)), 0, state.xer_so);
    R(state, 11) = memory.ReadU32(Address(R(state, 31)));
    R(state, 11) -= 1u;
    memory.WriteU32(Address(R(state, 31)), W(R(state, 11)));
    if (state.cr0.eq)
        memory.WriteU8(Address(R(state, 11)), static_cast<std::uint8_t>(R(state, 27)));
    else
    {
        R(state, 9) = memory.ReadU8(Address(R(state, 11)));
        R(state, 10) = std::uint64_t(std::int64_t(std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(R(state, 27)))));
        R(state, 9) = std::uint64_t(std::int64_t(std::bit_cast<std::int8_t>(
            static_cast<std::uint8_t>(R(state, 9)))));
        Compare<std::int32_t>(state.cr6, S(R(state, 9)), S(R(state, 10)), state.xer_so);
        if (!state.cr6.eq)
        {
            R(state, 11) += 1u;
            memory.WriteU32(Address(R(state, 31)), W(R(state, 11)));
            goto error;
        }
    }
    R(state, 10) = memory.ReadU32(Address(R(state, 31) + 12u));
    R(state, 9) = 1u;
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 4u));
    R(state, 3) = W(R(state, 27)) & 0xffu;
    R(state, 10) = (WordRotateMask(R(state, 9), 0, 1u) |
        (R(state, 10) & 0xfffffffffffffffeull));
    R(state, 11) += 1u;
    R(state, 10) = (WordRotateMask(R(state, 9), 0, 0x10u) |
        (R(state, 10) & 0xffffffffffffffefull));
    memory.WriteU32(Address(R(state, 31) + 4u), W(R(state, 11)));
    memory.WriteU32(Address(R(state, 31) + 12u), W(R(state, 10)));
    goto done;
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps,
    Registers& state)
{
    if (entry != 0x82b87cf8u) return false;
    Pushback(memory, deps, state);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_pushback_context
