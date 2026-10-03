#include "lo_semantics/object_field_routes.h"
#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>

namespace lo::semantic::gpu::object_field_routes
{
namespace
{
using recovery_abi::Address;

LoadedSingle LoadSingle(GuestMemory& memory, NativeServices& native,
    Registers& state, GuestAddress address)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if ((state.cached_fp_control & FlushMask) != 0u)
    {
        state.cached_fp_control &= ~FlushMask;
        native.SetHostFpControl(state.cached_fp_control);
    }
    return LoadedSingle::FromWord(memory.ReadU32(address));
}

Condition CompareSingles(std::uint64_t left_bits, std::uint64_t right_bits)
{
    const auto left = std::bit_cast<double>(left_bits);
    const auto right = std::bit_cast<double>(right_bits);
    const bool unordered = std::isnan(left) || std::isnan(right);
    return {std::uint8_t(!unordered && left < right),
        std::uint8_t(!unordered && left > right),
        std::uint8_t(!unordered && left == right), std::uint8_t(unordered)};
}

void MultiplyFromObject(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    recovery_abi::WriteU64(memory, Address(state.sp - 16u), state.r31);
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    state.r11 = memory.ReadU32(Address(state.r3 + 80u));
    state.r11 = memory.ReadU32(Address(state.r11 + 60u));
    state.r31 = memory.ReadU32(Address(state.r11));
    state.r3 = state.r31;
    state.r11 = memory.ReadU32(Address(state.r31));
    state.r11 = memory.ReadU32(Address(state.r11 + 288u));
    state.ctr = state.r11;
    state.lr = 0x822c4bd8u;
    native.CallGuest(Address(state.ctr) & ~3u, memory, state);
    const auto value = LoadSingle(memory, native, state,
        Address(state.r31 + 928u));
    state.f0_bits = value.FprBits();
    const auto product = std::bit_cast<double>(state.f1_bits) *
        std::bit_cast<double>(state.f0_bits);
    state.f1_bits = std::bit_cast<std::uint64_t>(
        double(float(product)));
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    state.r31 = recovery_abi::ReadU64(memory, Address(state.sp - 16u));
}

void ResolvePreferredSingle(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    state.r11 = 0xffffffff82000000ull;
    const auto first = LoadSingle(memory, native, state,
        Address(state.r3 + 916u));
    state.f0_bits = first.FprBits();
    const auto reference = LoadSingle(memory, native, state,
        Address(state.r11 + 3664u));
    state.f13_bits = reference.FprBits();
    state.cr6 = CompareSingles(state.f0_bits, state.f13_bits);
    if (state.cr6.eq != 0u)
    {
        auto selected = LoadSingle(memory, native, state,
            Address(state.r3 + 924u));
        state.f0_bits = selected.FprBits();
        state.cr6 = CompareSingles(state.f0_bits, state.f13_bits);
        if (state.cr6.eq != 0u)
        {
            selected = LoadSingle(memory, native, state,
                Address(state.r3 + 920u));
            state.f0_bits = selected.FprBits();
        }
        memory.WriteU32(Address(state.r3 + 916u), selected.StoreWord());
    }
    const auto result = LoadSingle(memory, native, state,
        Address(state.r3 + 916u));
    state.f1_bits = result.FprBits();
}

void RouteByFlag(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    state.r4 = state.r3;
    state.r11 = memory.ReadU32(Address(state.r4 + 84u)) & 0x20000000u;
    state.cr6 = {0, std::uint8_t(state.r11 != 0u),
        std::uint8_t(state.r11 == 0u), state.xer_so};
    if (state.cr6.eq != 0u)
    {
        state.r11 = memory.ReadU32(Address(state.r4));
        state.r11 = memory.ReadU32(Address(state.r11 + 308u));
    }
    else
    {
        state.r11 = memory.ReadU32(Address(state.r5));
        state.r3 = state.r5;
        state.r11 = memory.ReadU32(Address(state.r11 + 268u));
    }
    state.ctr = state.r11;
    native.CallGuest(Address(state.ctr) & ~3u, memory, state);
}

void RouteOptionalField(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    state.r11 = 0xffffffff82000000ull;
    state.r3 = memory.ReadU32(Address(state.r3 + 124u));
    state.cr6 = {0, std::uint8_t(state.r3 != 0u),
        std::uint8_t(state.r3 == 0u), state.xer_so};
    const auto value = LoadSingle(memory, native, state,
        Address(state.r11 + 30596u));
    state.f1_bits = value.FprBits();
    if (state.cr6.eq == 0u)
    {
        state.r5 = 1u;
        state.r4 = 1u;
        native.CallGuest(0x822c5e58u, memory, state);
    }
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    switch (entry)
    {
    case 0x822c4ba8u: MultiplyFromObject(memory, native, state); return true;
    case 0x822c4bf8u: ResolvePreferredSingle(memory, native, state); return true;
    case 0x822c5c68u: RouteByFlag(memory, native, state); return true;
    case 0x822c60d0u: RouteOptionalField(memory, native, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::object_field_routes
