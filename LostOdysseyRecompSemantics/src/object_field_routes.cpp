#include "lo_semantics/object_field_routes.h"
#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_child_float.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>

namespace lo::semantic::gpu::object_field_routes
{
namespace
{
using recovery_abi::Address;
namespace child = object_child_float;

child::Registers ToChild(const Registers& state)
{
    child::Registers result{};
    result.r[1] = state.sp; result.r[3] = state.r3;
    result.r[4] = state.r4; result.r[5] = state.r5;
    result.r[10] = state.r10; result.r[11] = state.r11;
    result.r[12] = state.r12; result.r[27] = state.r27;
    result.r[28] = state.r28; result.r[29] = state.r29;
    result.r[30] = state.r30; result.r[31] = state.r31;
    result.ctr = state.ctr; result.lr = state.lr;
    result.f0_bits = state.f0_bits; result.f1_bits = state.f1_bits;
    result.f13_bits = state.f13_bits;
    result.f30_bits = state.f30_bits; result.f31_bits = state.f31_bits;
    result.cached_fp_control = state.cached_fp_control;
    result.xer_so = state.xer_so; result.xer_ca = state.xer_ca;
    result.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.un};
    return result;
}
void FromChild(Registers& state, const child::Registers& result)
{
    state.sp = result.r[1]; state.r3 = result.r[3];
    state.r4 = result.r[4]; state.r5 = result.r[5];
    state.r10 = result.r[10]; state.r11 = result.r[11];
    state.r12 = result.r[12]; state.r27 = result.r[27];
    state.r28 = result.r[28]; state.r29 = result.r[29];
    state.r30 = result.r[30]; state.r31 = result.r[31];
    state.ctr = result.ctr; state.lr = result.lr;
    state.f0_bits = result.f0_bits; state.f1_bits = result.f1_bits;
    state.f13_bits = result.f13_bits;
    state.f30_bits = result.f30_bits; state.f31_bits = result.f31_bits;
    state.cached_fp_control = result.cached_fp_control;
    state.xer_so = result.xer_so; state.xer_ca = result.xer_ca;
    state.cr6 = {result.cr6.lt, result.cr6.gt,
        result.cr6.eq, result.cr6.un};
}
struct ChildServices final : child::NativeServices
{
    object_field_routes::NativeServices& native;
    object_field_routes::Registers& caller;
    ChildServices(object_field_routes::NativeServices& service,
        object_field_routes::Registers& state)
        : native(service), caller(state) {}
    void SetHostFpControl(std::uint32_t control) override
    { native.SetHostFpControl(control); }
    void CallGuest(GuestAddress target, GuestMemory& memory,
        child::Registers& state) override
    {
        object_field_routes::Registers exposed = caller;
        FromChild(exposed, state);
        native.CallGuest(target, memory, exposed);
        caller = exposed;
        const auto updated = ToChild(exposed);
        state.r[1] = updated.r[1]; state.r[3] = updated.r[3];
        state.r[4] = updated.r[4]; state.r[5] = updated.r[5];
        state.r[10] = updated.r[10]; state.r[11] = updated.r[11];
        state.r[12] = updated.r[12]; state.r[27] = updated.r[27];
        state.r[28] = updated.r[28]; state.r[29] = updated.r[29];
        state.r[30] = updated.r[30]; state.r[31] = updated.r[31];
        state.ctr = updated.ctr; state.lr = updated.lr;
        state.f0_bits = updated.f0_bits; state.f1_bits = updated.f1_bits;
        state.f13_bits = updated.f13_bits;
        state.f30_bits = updated.f30_bits; state.f31_bits = updated.f31_bits;
        state.cached_fp_control = updated.cached_fp_control;
        state.xer_so = updated.xer_so; state.xer_ca = updated.xer_ca;
        state.cr6 = updated.cr6;
    }
};

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
        auto lower = ToChild(state);
        ChildServices services(native, state);
        (void)child::Apply(0x822c5e58u, memory, services, lower);
        FromChild(state, lower);
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
