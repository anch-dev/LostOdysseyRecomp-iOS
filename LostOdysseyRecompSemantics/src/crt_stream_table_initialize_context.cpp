#include "lo_semantics/crt_stream_table_initialize_context.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::crt_stream_table_initialize_context
{
namespace
{
using recovery_abi::Address;
using Condition = crt_async_status_transfer::Condition;

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

void Initialize(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Address(R(state, 12)));
    const auto old_sp = R(state, 1);
    R(state, 1) -= 96u;
    memory.WriteU32(Address(R(state, 1)), Address(old_sp));
    R(state, 4) = 64u;
    R(state, 3) = 32u;
    state.lr = 0x82b81538u;
    (void)crt_record_allocation_context::Apply(0x82b81778u,
        memory, dependencies, state);
    Compare(state.cr0, R(state, 3), 0u, state.xer_so, false);
    if (state.cr0.eq)
    {
        R(state, 3) = UINT64_MAX;
    }
    else
    {
        R(state, 9) = 0xffffffff83380000ull;
        R(state, 10) = 32u;
        R(state, 8) = 0xffffffff83380000ull;
        R(state, 11) = R(state, 3);
        R(state, 7) = R(state, 3) + 2048u;
        memory.WriteU32(0x83378d68u, 32u);
        Compare(state.cr6, R(state, 3), R(state, 7),
            state.xer_so, false);
        R(state, 9) = 0u;
        memory.WriteU32(0x83378d80u, Address(R(state, 11)));
        if (state.cr6.lt)
        {
            R(state, 10) = 10u;
            do
            {
                R(state, 11) = UINT64_MAX;
                memory.WriteU8(Address(R(state, 3) + 4u), 0u);
                memory.WriteU8(Address(R(state, 3) + 5u), 10u);
                memory.WriteU32(Address(R(state, 3) + 8u), 0u);
                memory.WriteU8(Address(R(state, 3) + 40u), 0u);
                memory.WriteU8(Address(R(state, 3) + 41u), 10u);
                memory.WriteU32(Address(R(state, 3)), UINT32_MAX);
                memory.WriteU8(Address(R(state, 3) + 42u), 10u);
                R(state, 3) += 64u;
                R(state, 11) = memory.ReadU32(0x83378d80u);
                R(state, 7) = R(state, 11) + 2048u;
                Compare(state.cr6, R(state, 3), R(state, 7),
                    state.xer_so, false);
            } while (state.cr6.lt);
        }
        for (;;)
        {
            R(state, 11) += R(state, 9);
            R(state, 10) = UINT64_MAX - 62u;
            R(state, 7) = UINT64_MAX - 1u;
            R(state, 9) += 64u;
            Compare(state.cr6, R(state, 9), 192u,
                state.xer_so, true);
            memory.WriteU8(Address(R(state, 11) + 4u),
                Address(R(state, 10)) & 0xffu);
            memory.WriteU32(Address(R(state, 11)), Address(R(state, 7)));
            if (!state.cr6.lt) break;
            R(state, 11) = memory.ReadU32(0x83378d80u);
        }
        R(state, 3) = 0u;
    }
    R(state, 1) += 96u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (entry != 0x82b81520u) return false;
    Initialize(memory, dependencies, state);
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_table_initialize_context
