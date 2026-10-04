#include "lo_semantics/crt_stream_resize_context.h"

#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_resize_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_async_status_transfer::Condition;
using HeapRegisters = heap_block_query_context::Registers;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }
std::uint32_t Word(std::uint64_t value) { return Address(value); }
std::int32_t Signed(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(Word(value)); }

void Compare(Condition& cr, std::uint64_t lhs, std::uint64_t rhs,
    std::uint8_t so, bool signed_words)
{
    if (signed_words)
    {
        const auto a = Signed(lhs), b = Signed(rhs);
        cr = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), so};
    }
    else
    {
        const auto a = Word(lhs), b = Word(rhs);
        cr = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), so};
    }
}

HeapRegisters ToHeap(const Registers& state)
{
    HeapRegisters lower{};
    lower.r = state.r;
    lower.lr = state.lr;
    lower.ctr = state.ctr;
    lower.f0_bits = state.fpr_bits[0];
    lower.f1_bits = state.fpr_bits[1];
    lower.f13_bits = state.fpr_bits[13];
    lower.f30_bits = state.fpr_bits[30];
    lower.f31_bits = state.fpr_bits[31];
    lower.cached_fp_control = state.cached_fp_control;
    lower.xer_so = state.xer_so;
    lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt, state.cr0.gt, state.cr0.eq, state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    return lower;
}

void FromHeap(Registers& state, const HeapRegisters& lower)
{
    state.r = lower.r;
    state.lr = lower.lr;
    state.ctr = lower.ctr;
    state.fpr_bits[0] = lower.f0_bits;
    state.fpr_bits[1] = lower.f1_bits;
    state.fpr_bits[13] = lower.f13_bits;
    state.fpr_bits[30] = lower.f30_bits;
    state.fpr_bits[31] = lower.f31_bits;
    state.cached_fp_control = lower.cached_fp_control;
    state.xer_so = lower.xer_so;
    state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt, lower.cr0.gt, lower.cr0.eq, lower.cr0.un};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt, lower.cr6.eq, lower.cr6.un};
}

void Accepted(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (!crt_record_allocation_context::ApplyAcceptedLower(entry, memory,
        dependencies.reallocation.allocation, state))
        throw std::logic_error("missing accepted resize lower");
}

void QueryAllocationSize(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    const auto old_sp = R(state, 1);
    R(state, 1) -= 96u;
    memory.WriteU32(Address(R(state, 1)), Word(old_sp));
    R(state, 31) = R(state, 3);
    Compare(state.cr6, R(state, 31), 0u, state.xer_so, false);
    if (state.cr6.eq)
    {
        state.lr = 0x82b82120u;
        Accepted(0x82b7fd78u, memory, dependencies, state);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22u;
        R(state, 7) = R(state, 6) = R(state, 5) =
            R(state, 4) = R(state, 3) = 0u;
        memory.WriteU32(Address(R(state, 11)), 22u);
        state.lr = 0x82b82144u;
        Accepted(0x82b7fec0u, memory, dependencies, state);
        R(state, 3) = UINT64_MAX;
    }
    else
    {
        state.lr = 0x82b82150u;
        Accepted(0x823acc98u, memory, dependencies, state);
        R(state, 4) = 0u;
        R(state, 5) = R(state, 31);
        state.lr = 0x82b8215cu;
        auto lower = ToHeap(state);
        if (!heap_block_query_context::Apply(0x827cc218u, memory,
            dependencies.heap_native, lower))
            throw std::logic_error("missing heap size query");
        FromHeap(state, lower);
    }
    R(state, 1) += 96u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 16u));
}

void Save29(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 29u; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
    state.lr = 0x82b7d338u;
}

void Restore29(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 29u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(R(state, 1) - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

void FillZeroTail(GuestMemory& memory, Registers& state)
{
    const auto destination = Word(R(state, 3));
    const auto bytes = Word(R(state, 5));
    const auto padding = (0u - destination) & 3u;
    const auto prefix = bytes < padding ? bytes : padding;
    const auto remaining = bytes - prefix;
    (void)FillGuestMemory(memory, destination, Word(R(state, 4)), bytes);
    // Accepted 82B7BC40 leaf ABI: preserve r3, including its high word.
    R(state, 6) = R(state, 3) + prefix + (remaining & ~3u);
    R(state, 5) -= prefix;
    R(state, 4) = (R(state, 4) & 0xffffffff00000000ull) |
        (std::uint32_t{static_cast<std::uint8_t>(R(state, 4))} *
            0x01010101u);
    const auto tail = remaining & 3u;
    R(state, 0) = tail;
    Compare(state.cr0, tail, 0u, state.xer_so, true);
    state.ctr = tail == 3u ? 1u : 0u;
}

void Resize(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    Save29(memory, state);
    const auto old_sp = R(state, 1);
    R(state, 1) -= 112u;
    memory.WriteU32(Address(R(state, 1)), Word(old_sp));
    R(state, 31) = R(state, 3);
    R(state, 29) = 0u;
    Compare(state.cr6, R(state, 4), 0u, state.xer_so, false);
    if (!state.cr6.eq)
    {
        R(state, 11) = UINT64_MAX - 4095u;
        R(state, 11) = (R(state, 11) & 0xffffffff00000000ull) |
            (Word(R(state, 11)) / Word(R(state, 4)));
        Compare(state.cr6, R(state, 11), R(state, 5),
            state.xer_so, false);
        if (state.cr6.lt)
        {
            state.lr = 0x82b7d364u;
            Accepted(0x82b7fd78u, memory, dependencies, state);
            R(state, 11) = R(state, 3);
            R(state, 10) = 12u;
            R(state, 7) = R(state, 6) = R(state, 5) =
                R(state, 4) = R(state, 3) = 0u;
            memory.WriteU32(Address(R(state, 11)), 12u);
            state.lr = 0x82b7d388u;
            Accepted(0x82b7fec0u, memory, dependencies, state);
            R(state, 3) = 0u;
            goto done;
        }
    }
    R(state, 30) = static_cast<std::uint64_t>(
        std::int64_t{Signed(R(state, 4))} * Signed(R(state, 5)));
    Compare(state.cr6, R(state, 31), 0u, state.xer_so, false);
    if (!state.cr6.eq)
    {
        R(state, 3) = R(state, 31);
        state.lr = 0x82b7d3a4u;
        QueryAllocationSize(memory, dependencies, state);
        R(state, 29) = R(state, 3);
    }
    R(state, 4) = R(state, 30);
    R(state, 3) = R(state, 31);
    state.lr = 0x82b7d3b4u;
    if (!crt_reallocation_context::Apply(0x823acAD8u, memory,
        dependencies.reallocation, state))
        throw std::logic_error("missing CRT reallocation lower");
    R(state, 31) = R(state, 3);
    Compare(state.cr0, R(state, 31), 0u, state.xer_so, true);
    if (!state.cr0.eq)
    {
        Compare(state.cr6, R(state, 29), R(state, 30),
            state.xer_so, false);
        if (state.cr6.lt)
        {
            R(state, 5) = R(state, 30) - R(state, 29);
            R(state, 4) = 0u;
            R(state, 3) = R(state, 29) + R(state, 31);
            state.lr = 0x82b7d3d4u;
            FillZeroTail(memory, state);
        }
    }
    R(state, 3) = R(state, 31);
done:
    R(state, 1) += 112u;
    Restore29(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82b82100u: QueryAllocationSize(memory, dependencies, state);
        return true;
    case 0x82b7d330u: Resize(memory, dependencies, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_resize_context
