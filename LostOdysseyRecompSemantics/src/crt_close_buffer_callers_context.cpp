#include "lo_semantics/crt_close_buffer_callers_context.h"

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_close_buffer_callers_context
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_async_status_transfer::Condition;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }
std::uint32_t Word(std::uint64_t value) { return Address(value); }
void Compare(Condition& cr, std::uint64_t lhs, std::uint64_t rhs,
    std::uint8_t so)
{
    const auto a = Word(lhs), b = Word(rhs);
    cr = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), so};
}
void Push(GuestMemory& memory, Registers& state, unsigned bytes)
{
    const auto old_sp = R(state, 1);
    R(state, 1) -= bytes;
    memory.WriteU32(Address(R(state, 1)), Word(old_sp));
}
void Save28(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 28u; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
    state.lr = 0x82bd0e00u;
}
void Restore28(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 28u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(R(state, 1) - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}

void DirectBlock(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (!crt_close_block_output_context::Apply(entry, memory,
        dependencies.output, state))
        throw std::logic_error("missing selected block output lower");
}

void DirectRecursive(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    if (!crt_close_recursive_buffer_context::Apply(0x82bd0c18u,
        memory, dependencies.recursive_guest, state))
        throw std::logic_error("missing selected recursive buffer lower");
}

void DirectClose(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    auto lower = crt_context_adapter::ToStream(state);
    if (!crt_stream_bulk_close_routes::Apply(0x82b85d88u, memory,
        dependencies.output.accepted.close, lower))
        throw std::logic_error("missing accepted bulk close lower");
    crt_context_adapter::FromStream(state, lower);
}

void WriteNode(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    Push(memory, state, 96u);
    R(state, 31) = memory.ReadU32(Address(R(state, 4) + 4u));
    R(state, 6) = R(state, 5);
    Compare(state.cr6, R(state, 31), 0u, state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 3) = memory.ReadU32(Address(R(state, 4)));
        R(state, 5) = R(state, 31);
        R(state, 4) = 1u;
        state.lr = 0x82bd08e0u;
        DirectBlock(0x82df27b0u, memory, dependencies, state);
        R(state, 11) = R(state, 31) - R(state, 3);
        R(state, 11) = std::countl_zero(Word(R(state, 11)));
        R(state, 3) = recovery_abi::WordRotateMask(R(state, 11), 27, 1u);
    }
    else R(state, 3) = 1u;
    R(state, 1) += 96u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 16u));
}

void WriteChain(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    Save28(memory, state);
    Push(memory, state, 128u);
    R(state, 28) = R(state, 3);
    R(state, 29) = R(state, 4);
    state.lr = 0x82bd0e10u;
    DirectRecursive(memory, dependencies, state);
    R(state, 30) = memory.ReadU32(Address(R(state, 28) + 4u));
    R(state, 11) = memory.ReadU32(Address(R(state, 30) + 12u));
    Compare(state.cr6, R(state, 11), 0u, state.xer_so);
    if (state.cr6.eq) goto last;
    for (;;)
    {
        R(state, 31) = memory.ReadU32(Address(R(state, 30) + 4u));
        Compare(state.cr6, R(state, 31), 0u, state.xer_so);
        if (state.cr6.eq) R(state, 11) = 1u;
        else
        {
            R(state, 6) = R(state, 29);
            R(state, 3) = memory.ReadU32(Address(R(state, 30)));
            R(state, 5) = R(state, 31);
            R(state, 4) = 1u;
            state.lr = 0x82bd0e48u;
            DirectBlock(0x82df27b0u, memory, dependencies, state);
            R(state, 11) = R(state, 31) - R(state, 3);
            R(state, 11) = std::countl_zero(Word(R(state, 11)));
            R(state, 11) = recovery_abi::WordRotateMask(R(state, 11),
                27, 1u);
        }
        R(state, 11) = Word(R(state, 11)) & 0xffu;
        Compare(state.cr6, R(state, 11), 0u, state.xer_so);
        if (state.cr6.eq)
        {
            R(state, 3) = 0u;
            goto done;
        }
        R(state, 30) = memory.ReadU32(Address(R(state, 30) + 12u));
        R(state, 11) = memory.ReadU32(Address(R(state, 30) + 12u));
        Compare(state.cr6, R(state, 11), 0u, state.xer_so);
        if (state.cr6.eq) break;
    }
last:
    R(state, 5) = R(state, 29);
    R(state, 4) = R(state, 30);
    R(state, 3) = R(state, 28);
    state.lr = 0x82bd0e80u;
    WriteNode(memory, dependencies, state);
    R(state, 11) = Word(R(state, 3)) & 0xffu;
    R(state, 11) = std::countl_zero(Word(R(state, 11)));
    R(state, 11) = recovery_abi::WordRotateMask(R(state, 11), 27, 1u);
    R(state, 3) = R(state, 11) ^ 1u;
done:
    R(state, 1) += 128u;
    Restore28(memory, state);
}

void Close(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 24u), R(state, 30));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    Push(memory, state, 112u);
    R(state, 30) = R(state, 3);
    R(state, 3) = R(state, 4);
    Compare(state.cr6, R(state, 5), 0u, state.xer_so);
    if (!state.cr6.eq) R(state, 4) = R(state, 5);
    else
    {
        R(state, 11) = std::uint64_t(std::int64_t(-2113077248));
        R(state, 4) = R(state, 11) + 24644u;
    }
    state.lr = 0x82bd1238u;
    if (!crt_stream_close_shared_lower::Apply(0x82df2150u, memory,
        dependencies.output.accepted, state))
        throw std::logic_error("missing accepted close shared lower");
    R(state, 31) = R(state, 3);
    Compare(state.cr6, R(state, 31), 0u, state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 4) = R(state, 31);
        R(state, 3) = R(state, 30);
        state.lr = 0x82bd1250u;
        WriteChain(memory, dependencies, state);
        R(state, 30) = R(state, 3);
        R(state, 3) = R(state, 31);
        state.lr = 0x82bd125cu;
        DirectClose(memory, dependencies, state);
        R(state, 3) = R(state, 30);
    }
    R(state, 1) += 112u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 30) = ReadU64(memory, Address(R(state, 1) - 24u));
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 16u));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82bd0898u: WriteNode(memory, dependencies, state); return true;
    case 0x82bd0df8u: WriteChain(memory, dependencies, state); return true;
    case 0x82bd1200u: Close(memory, dependencies, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_close_buffer_callers_context
