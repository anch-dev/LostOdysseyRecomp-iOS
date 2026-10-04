#include "lo_semantics/crt_stream_buffer_growth_callers.h"

#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_buffer_growth_callers
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
using Condition = crt_async_status_transfer::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t Word(std::uint64_t value) { return Address(value); }
void CompareWord(Condition& condition, std::uint64_t left,
    std::uint64_t right, std::uint8_t so)
{
    const auto a = Word(left), b = Word(right);
    condition = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), so};
}
void Save28(GuestMemory& memory, Registers& state, GuestAddress return_address)
{
    R(state, 12) = state.lr;
    state.lr = return_address;
    for (unsigned index = 28u; index <= 31u; ++index)
        WriteU64(memory, Address(R(state, 1) - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
}
void Restore28(GuestMemory& memory, Registers& state)
{
    for (unsigned index = 28u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(R(state, 1) - 8u * (33u - index)));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}
void Allocate(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    if (!crt_record_allocation_context::Apply(0x82b81778u, memory,
            dependencies.allocation, state))
        throw std::logic_error("missing accepted CRT record allocator");
}
void Resize(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    if (!crt_stream_resize_context::Apply(0x82b7d330u, memory,
            dependencies.resize, state))
        throw std::logic_error("missing selected CRT stream resize");
}

void Grow(GuestAddress entry, unsigned width, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    Save28(memory, state, entry == 0x82df4a58u ?
        0x82df4a60u : 0x82b824b8u);
    const auto old_sp = R(state, 1);
    R(state, 1) -= 128u;
    memory.WriteU32(Word(R(state, 1)), Word(old_sp));
    R(state, 31) = R(state, 4);
    R(state, 30) = R(state, 5);
    R(state, 29) = R(state, 6);
    R(state, 28) = R(state, 7);
    R(state, 11) = memory.ReadU32(Word(R(state, 31)));
    CompareWord(state.cr6, R(state, 3), R(state, 11), state.xer_so);
    if (!state.cr6.eq) goto success;
    R(state, 3) = memory.ReadU32(Word(R(state, 30)));
    CompareWord(state.cr6, R(state, 3), R(state, 29), state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 5) = width;
        R(state, 4) = R(state, 11);
        state.lr = entry == 0x82df4a58u ? 0x82df4ad4u : 0x82b82530u;
        Resize(memory, dependencies, state);
        CompareWord(state.cr0, R(state, 3), 0u, state.xer_so);
        if (state.cr0.eq) goto failed;
        memory.WriteU32(Word(R(state, 30)), Word(R(state, 3)));
    }
    else
    {
        R(state, 4) = width;
        R(state, 3) = R(state, 11);
        state.lr = entry == 0x82df4a58u ? 0x82df4a98u : 0x82b824f0u;
        Allocate(memory, dependencies, state);
        CompareWord(state.cr0, R(state, 3), 0u, state.xer_so);
        memory.WriteU32(Word(R(state, 30)), Word(R(state, 3)));
        if (state.cr0.eq) goto failed;
        R(state, 11) = 1u;
        R(state, 4) = R(state, 29);
        memory.WriteU32(Word(R(state, 28)), Word(R(state, 11)));
        if (width == 4u)
        {
            R(state, 11) = memory.ReadU32(Word(R(state, 31)));
            R(state, 3) = memory.ReadU32(Word(R(state, 30)));
            R(state, 5) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
        }
        else
        {
            R(state, 5) = memory.ReadU32(Word(R(state, 31)));
            R(state, 3) = memory.ReadU32(Word(R(state, 30)));
        }
        state.lr = entry == 0x82df4a58u ? 0x82df4ac4u : 0x82b82520u;
        if (!crt_copy_full_context::Apply(0x82b7a0b0u, memory, state))
            throw std::logic_error("missing full-context guest copy");
    }
    R(state, 11) = memory.ReadU32(Word(R(state, 31)));
    R(state, 11) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
    memory.WriteU32(Word(R(state, 31)), Word(R(state, 11)));
success:
    R(state, 3) = 1u;
    goto done;
failed:
    R(state, 3) = 0u;
done:
    R(state, 1) += 128u;
    Restore28(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82df4a58u: Grow(entry, 2u, memory, dependencies, state); return true;
    case 0x82b824b0u: Grow(entry, 4u, memory, dependencies, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_buffer_growth_callers
