#include "lo_semantics/crt_stream_open_wrapper.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_open_wrapper
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_async_status_transfer::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t Word(std::uint64_t value) { return Address(value); }
std::int32_t Signed(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(Word(value)); }
void Compare(Condition& condition, std::int32_t left, std::int32_t right,
    std::uint8_t so)
{
    condition = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), so};
}
void OpenLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    if (!crt_stream_open_pipeline::ApplyLower(entry, memory,
            dependencies.open, state))
        throw std::logic_error("missing accepted CRT open wrapper lower");
}
void Unlock(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp = R(state, 1); lower.lr = state.lr;
    lower.r3 = R(state, 3); lower.r9 = R(state, 9);
    lower.r10 = R(state, 10); lower.r11 = R(state, 11);
    lower.r12 = R(state, 12); lower.r30 = R(state, 30);
    lower.r31 = R(state, 31); lower.xer_ca = state.xer_ca;
    if (!crt_stream_pointer_unlock::Apply(0x82b863f0u, memory,
            dependencies.unlock, lower))
        throw std::logic_error("missing accepted CRT pointer unlock");
    R(state, 1) = lower.sp; state.lr = lower.lr;
    R(state, 3) = lower.r3; R(state, 9) = lower.r9;
    R(state, 10) = lower.r10; R(state, 11) = lower.r11;
    R(state, 12) = lower.r12; R(state, 30) = lower.r30;
    R(state, 31) = lower.r31; state.xer_ca = lower.xer_ca;
}

void Cleanup(GuestMemory& memory, Dependencies dependencies,
    Registers& state, bool load_saved_output)
{
    // The 6868 entry loads r30 from the parent frame; 6888 preserves its
    // incoming r30. Both enter the shared tail at 82DF68A0.
    WriteU64(memory, Address(R(state, 1) - 8u), R(state, 31));
    R(state, 31) = R(state, 12) - 112u;
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 30));
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 24u), Word(R(state, 12)));
    const auto next_sp = R(state, 1) - 112u;
    memory.WriteU32(Address(next_sp), Word(R(state, 1)));
    R(state, 1) = next_sp;
    if (load_saved_output)
        R(state, 30) = memory.ReadU32(Address(R(state, 31) + 164u));

    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 80u));
    Compare(state.cr6, Signed(R(state, 11)), 0, state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 31) + 84u));
        Compare(state.cr6, Signed(R(state, 11)), 0, state.xer_so);
        if (!state.cr6.eq)
        {
            R(state, 11) = memory.ReadU32(Word(R(state, 30)));
            R(state, 10) = -2093481984;
            R(state, 10) += -29312;
            R(state, 9) = std::rotl(std::uint64_t{Word(R(state, 11))} |
                (R(state, 11) << 32), 6) & 0x7c0u;
            state.xer_ca = std::uint8_t(Signed(R(state, 11)) < 0 &&
                (Word(R(state, 11)) & 31u) != 0);
            R(state, 11) = Signed(R(state, 11)) >> 5;
            R(state, 11) = std::rotl(std::uint64_t{Word(R(state, 11))} |
                (R(state, 11) << 32), 2) & 0xfffffffcu;
            R(state, 11) = memory.ReadU32(Address(R(state, 11) + R(state, 10)));
            R(state, 11) += R(state, 9);
            R(state, 10) = memory.ReadU8(Address(R(state, 11) + 4u));
            R(state, 10) = static_cast<std::uint64_t>(static_cast<std::int64_t>(
                static_cast<std::int8_t>(Word(R(state, 10)))));
            R(state, 10) &= 0xfffffffeu;
            memory.WriteU8(Address(R(state, 11) + 4u),
                static_cast<std::uint8_t>(Word(R(state, 10))));
        }
        R(state, 3) = memory.ReadU32(Word(R(state, 30)));
        state.lr = 0x82df68f0u;
        Unlock(memory, dependencies, state);
    }
    R(state, 1) = memory.ReadU32(Word(R(state, 1)));
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 8u));
    R(state, 30) = ReadU64(memory, Address(R(state, 1) - 16u));
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 24u));
    state.lr = R(state, 12);
}

void Open(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Word(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 24u), R(state, 30));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    R(state, 31) = R(state, 1) - 112u;
    memory.WriteU32(Word(R(state, 31)), Word(R(state, 1)));
    R(state, 1) = R(state, 31);
    R(state, 30) = R(state, 7);
    memory.WriteU32(Address(R(state, 31) + 164u), Word(R(state, 30)));
    R(state, 9) = R(state, 8);
    R(state, 11) = Word(R(state, 30)) == 0 ? 0u : 1u;
    R(state, 10) = 0;
    memory.WriteU32(Address(R(state, 31) + 80u), 0u);
    Compare(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
    if (state.cr0.eq) goto invalid;

    R(state, 11) = Word(R(state, 3)) == 0 ? 0u : 1u;
    R(state, 10) = UINT64_MAX;
    memory.WriteU32(Word(R(state, 30)), Word(R(state, 10)));
    Compare(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
    if (state.cr0.eq) goto invalid;
    Compare(state.cr6, Signed(R(state, 9)), 0, state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 11) = Word(R(state, 6)) & 0xfffffe7fu;
        R(state, 11) = Word(R(state, 11)) == 0 ? 1u : 0u;
        Compare(state.cr0, Signed(R(state, 11)), 0, state.xer_so);
        if (state.cr0.eq) goto invalid;
    }
    R(state, 8) = R(state, 6);
    R(state, 7) = R(state, 5);
    R(state, 6) = R(state, 4);
    R(state, 5) = R(state, 3);
    R(state, 4) = R(state, 30);
    R(state, 3) = R(state, 31) + 80u;
    state.lr = 0x82df6828u;
    if (!crt_stream_open_pipeline::Apply(0x82df6240u, memory,
            dependencies.open, state))
        throw std::logic_error("missing accepted CRT open pipeline");
    memory.WriteU32(Address(R(state, 31) + 84u), Word(R(state, 3)));
    R(state, 12) = R(state, 31) + 112u;
    state.lr = 0x82df6838u;
    Cleanup(memory, dependencies, state, false);
    R(state, 3) = memory.ReadU32(Address(R(state, 31) + 84u));
    Compare(state.cr6, Signed(R(state, 3)), 0, state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 11) = UINT64_MAX;
        R(state, 10) = memory.ReadU32(Address(R(state, 31) + 164u));
        memory.WriteU32(Word(R(state, 10)), Word(R(state, 11)));
    }
    goto done;
invalid:
    state.lr = 0x82df67a4u;
    OpenLower(0x82b7fd78u, memory, dependencies, state);
    R(state, 11) = R(state, 3);
    R(state, 10) = 22u;
    for (unsigned index = 3; index <= 7; ++index) R(state, index) = 0;
    memory.WriteU32(Word(R(state, 11)), Word(R(state, 10)));
    state.lr = 0x82df67c8u;
    OpenLower(0x82b7fec0u, memory, dependencies, state);
    R(state, 3) = 22u;
done:
    R(state, 1) = R(state, 31) + 112u;
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
    case 0x82df6760u: Open(memory, dependencies, state); return true;
    case 0x82df6868u: Cleanup(memory, dependencies, state, true); return true;
    case 0x82df6888u: Cleanup(memory, dependencies, state, false); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_open_wrapper
