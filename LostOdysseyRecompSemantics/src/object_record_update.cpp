#include "lo_semantics/object_record_update.h"
#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <limits>

namespace lo::semantic::gpu::object_record_update
{
namespace
{
using recovery_abi::Address;
std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void Compare(Registers& state, std::uint64_t value, std::uint32_t bound)
{
    const auto word = Address(value);
    state.cr6 = {std::uint8_t(word < bound), std::uint8_t(word > bound),
        std::uint8_t(word == bound), state.xer_so};
}

void DisableFlush(NativeServices& native, Registers& state)
{
    constexpr std::uint32_t Mask = 0x8040u;
    if (state.cached_fp_control & Mask)
    {
        state.cached_fp_control &= ~Mask;
        native.SetHostFpControl(state.cached_fp_control);
    }
}

void CheckFlags(GuestMemory& memory, Registers& state)
{
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 112u)) & 0x80000u;
    Compare(state, R(state, 11), 0);
    if (!state.cr6.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 4) + 116u)) & 0x4000u;
        Compare(state, R(state, 11), 0);
        if (!state.cr6.eq)
        {
            R(state, 11) = memory.ReadU8(Address(R(state, 4) + 84u));
            Compare(state, R(state, 11), 10);
            if (state.cr6.eq) { R(state, 3) = 1; return; }
            Compare(state, R(state, 11), 7);
            if (state.cr6.eq) { R(state, 3) = 1; return; }
        }
    }
    R(state, 3) = 0;
}

void UpdateRecord(GuestMemory& memory, NativeServices& native, Registers& state)
{
    R(state, 10) = 0xffffffff82000000ull;
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 528u));
    Compare(state, R(state, 11), 0);
    DisableFlush(native, state);
    state.f0_bits = LoadedSingle::FromWord(
        memory.ReadU32(Address(R(state, 10) + 3664u))).FprBits();
    if (!state.cr6.eq)
    {
        R(state, 10) = R(state, 11) + 260u;
        R(state, 11) += 760u;
        R(state, 9) = memory.ReadU32(Address(R(state, 10)));
        memory.WriteU32(Address(R(state, 11)), Address(R(state, 9)));
        R(state, 9) = memory.ReadU32(Address(R(state, 10) + 4u));
        memory.WriteU32(Address(R(state, 11) + 4u), Address(R(state, 9)));
        R(state, 10) = memory.ReadU32(Address(R(state, 10) + 8u));
        memory.WriteU32(Address(R(state, 11) + 8u), Address(R(state, 10)));

        R(state, 11) = memory.ReadU32(Address(R(state, 3) + 528u));
        R(state, 9) = memory.ReadU32(Address(R(state, 5)));
        R(state, 10) = memory.ReadU32(Address(R(state, 11) + 268u));
        R(state, 8) = memory.ReadU32(Address(R(state, 11) + 264u));
        const auto signed_word = std::bit_cast<std::int32_t>(Address(R(state, 10)));
        R(state, 10) = static_cast<std::uint64_t>(static_cast<std::int64_t>(signed_word));
        recovery_abi::WriteU64(memory, Address(state.sp - 16u), R(state, 10));
        R(state, 10) = memory.ReadU32(Address(R(state, 11) + 260u));
        R(state, 10) += R(state, 9);
        R(state, 9) = memory.ReadU32(Address(R(state, 11) + 268u));
        memory.WriteU32(Address(R(state, 11) + 260u), Address(R(state, 10)));
        R(state, 10) = memory.ReadU32(Address(R(state, 5) + 4u));
        R(state, 10) += R(state, 8);
        memory.WriteU32(Address(R(state, 11) + 264u), Address(R(state, 10)));
        R(state, 10) = memory.ReadU32(Address(R(state, 5) + 8u));
        R(state, 10) += R(state, 9);
        memory.WriteU32(Address(R(state, 11) + 268u), Address(R(state, 10)));

        const auto spilled = std::bit_cast<std::int64_t>(
            recovery_abi::ReadU64(memory, Address(state.sp - 16u)));
        state.f0_bits = std::bit_cast<std::uint64_t>(
            double(float(double(spilled))));
    }
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 548u)) & 0x2000000u;
    Compare(state, R(state, 11), 0);
    if (!state.cr6.eq) return;
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 268u));
    memory.WriteU32(Address(R(state, 4) + 8u), Address(R(state, 11)));
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 528u));
    Compare(state, R(state, 11), 0);
    if (state.cr6.eq) return;
    R(state, 11) += 268u;
    DisableFlush(native, state);
    const auto value = std::bit_cast<double>(state.f0_bits);
    const auto integer = value > double(std::numeric_limits<std::int32_t>::max()) ?
        std::numeric_limits<std::int32_t>::max() :
        (std::isnan(value) || value < double(std::numeric_limits<std::int32_t>::min())) ?
        std::numeric_limits<std::int32_t>::min() : static_cast<std::int32_t>(value);
    state.f0_bits = static_cast<std::uint64_t>(static_cast<std::int64_t>(integer));
    memory.WriteU32(Address(R(state, 11)), Address(state.f0_bits));
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    switch (entry)
    {
    case 0x8237dc28u: CheckFlags(memory, state); return true;
    case 0x8237dc70u: UpdateRecord(memory, native, state); return true;
    default: return false;
    }
}
}
