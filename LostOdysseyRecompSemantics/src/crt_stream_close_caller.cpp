#include "lo_semantics/crt_stream_close_caller.h"

#include "lo_semantics/crt_stream_pointer_unlock.h"
#include "lo_semantics/recovery_abi.h"

#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_close_caller
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

constexpr GuestAddress kCaller = 0x82b87b18u;
constexpr GuestAddress kUnlock = 0x82b87c54u;
constexpr GuestAddress kCount = 0x83378d68u;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }

void Compare(crt_stream_operations::Condition& condition,
    std::uint64_t left, std::uint64_t right, std::uint8_t so,
    bool signed_words)
{
    const auto a = Address(left);
    const auto b = Address(right);
    if (signed_words)
    {
        const auto x = static_cast<std::int32_t>(a);
        const auto y = static_cast<std::int32_t>(b);
        condition = {std::uint8_t(x < y), std::uint8_t(x > y),
            std::uint8_t(x == y), so};
    }
    else
        condition = {std::uint8_t(a < b), std::uint8_t(a > b),
            std::uint8_t(a == b), so};
}

void Accepted(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    if (!crt_stream_operations::ApplyAcceptedCallee(address, memory,
        dependencies.accepted, state))
        throw std::logic_error("missing accepted CRT stream callee");
}

void PointerLeaf(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    crt_stream_pointer_unlock::Registers lower{};
    lower.sp = state.sp; lower.lr = state.lr;
    lower.r3 = R(state, 3); lower.r9 = R(state, 9);
    lower.r10 = R(state, 10); lower.r11 = R(state, 11);
    lower.r12 = R(state, 12); lower.r30 = R(state, 30);
    lower.r31 = R(state, 31); lower.xer_ca = state.xer_ca;
    if (!crt_stream_pointer_unlock::Apply(0x82b863f0u, memory,
        dependencies.accepted.unlock, lower))
        throw std::logic_error("missing accepted pointer-unlock callee");
    state.sp = lower.sp; state.lr = lower.lr;
    R(state, 3) = lower.r3; R(state, 9) = lower.r9;
    R(state, 10) = lower.r10; R(state, 11) = lower.r11;
    R(state, 12) = lower.r12; R(state, 30) = lower.r30;
    R(state, 31) = lower.r31; state.xer_ca = lower.xer_ca;
}

void Unlock(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    WriteU64(memory, Address(state.sp - 8u), R(state, 31));
    R(state, 31) = R(state, 12) - 144u;
    WriteU64(memory, Address(state.sp - 16u), R(state, 30));
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 24u), Address(R(state, 12)));
    memory.WriteU32(Address(state.sp - 112u), Address(state.sp));
    state.sp -= 112u;
    R(state, 3) = R(state, 30);
    state.lr = 0x82b87c74u;
    PointerLeaf(memory, dependencies, state);
    state.sp = memory.ReadU32(Address(state.sp));
    R(state, 31) = ReadU64(memory, Address(state.sp - 8u));
    R(state, 30) = ReadU64(memory, Address(state.sp - 16u));
    R(state, 12) = memory.ReadU32(Address(state.sp - 24u));
    state.lr = R(state, 12);
}

void EnterCaller(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 27; index <= 31; ++index)
        WriteU64(memory, Address(state.sp - 16u - 8u * (31u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    state.lr = 0x82b87b20u;
    R(state, 31) = state.sp - 144u;
    memory.WriteU32(Address(state.sp - 144u), Address(state.sp));
    state.sp -= 144u;
}

void LeaveCaller(GuestMemory& memory, Registers& state)
{
    state.sp = R(state, 31) + 144u;
    for (unsigned index = 27; index <= 31; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 16u - 8u * (31u - index)));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void InvalidHandle(GuestMemory& memory, Dependencies dependencies,
    Registers& state, bool invalid_parameter)
{
    Accepted(0x82b7fdb0u, memory, dependencies, state,
        invalid_parameter ? 0x82b87b78u : 0x82b87b3cu);
    R(state, 11) = 0;
    memory.WriteU32(Address(R(state, 3)), 0);
    Accepted(0x82b7fd78u, memory, dependencies, state,
        invalid_parameter ? 0x82b87b84u : 0x82b87b48u);
    R(state, 11) = R(state, 3);
    R(state, 10) = 9;
    if (invalid_parameter)
        R(state, 7) = R(state, 6) = R(state, 5) =
            R(state, 4) = R(state, 3) = 0;
    else
        R(state, 3) = UINT64_MAX;
    memory.WriteU32(Address(R(state, 11)), 9u);
    if (invalid_parameter)
    {
        Accepted(0x82b7fec0u, memory, dependencies, state, 0x82b87ba8u);
        R(state, 3) = UINT64_MAX;
    }
}

void Caller(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    EnterCaller(memory, state);
    R(state, 30) = R(state, 3);
    memory.WriteU32(Address(R(state, 31) + 164u), Address(R(state, 30)));
    Compare(state.cr6, R(state, 30), 0xfffffffeu, state.xer_so, true);
    if (state.cr6.eq)
    {
        InvalidHandle(memory, dependencies, state, false);
        LeaveCaller(memory, state);
        return;
    }
    Compare(state.cr6, R(state, 30), 0, state.xer_so, true);
    bool valid = !state.cr6.lt;
    if (valid)
    {
        R(state, 11) = 0xffffffff83380000ull;
        R(state, 11) = memory.ReadU32(kCount);
        Compare(state.cr6, R(state, 30), R(state, 11), state.xer_so, false);
        valid = state.cr6.lt;
    }
    if (valid)
    {
        R(state, 11) = 0xffffffff83380000ull;
        R(state, 29) = R(state, 11) - 29312u;
        const auto index = R(state, 30);
        state.xer_ca = (static_cast<std::int32_t>(Address(index)) < 0) &&
            ((Address(index) & 31u) != 0);
        R(state, 11) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int32_t>(Address(index)) >> 5));
        R(state, 27) = WordRotateMask(R(state, 11), 2, 0xfffffffcu);
        R(state, 28) = WordRotateMask(index, 6, 0x7c0u);
        R(state, 11) = memory.ReadU32(Address(R(state, 27) + R(state, 29)));
        R(state, 11) += R(state, 28);
        R(state, 11) = memory.ReadU8(Address(R(state, 11) + 4u));
        R(state, 11) &= 1u;
        Compare(state.cr0, R(state, 11), 0, state.xer_so, true);
        valid = !state.cr0.eq;
    }
    if (!valid)
    {
        InvalidHandle(memory, dependencies, state, true);
        LeaveCaller(memory, state);
        return;
    }
    R(state, 3) = R(state, 30);
    Accepted(0x82b862f8u, memory, dependencies, state, 0x82b87be0u);
    R(state, 11) = memory.ReadU32(Address(R(state, 27) + R(state, 29)));
    R(state, 11) += R(state, 28);
    R(state, 11) = memory.ReadU8(Address(R(state, 11) + 4u));
    R(state, 11) &= 1u;
    Compare(state.cr0, R(state, 11), 0, state.xer_so, true);
    if (state.cr0.eq)
    {
        Accepted(0x82b7fd78u, memory, dependencies, state, 0x82b87c0cu);
        R(state, 11) = 9;
        memory.WriteU32(Address(R(state, 3)), 9u);
        R(state, 11) = UINT64_MAX;
        memory.WriteU32(Address(R(state, 31) + 80u), Address(R(state, 11)));
    }
    else
    {
        R(state, 3) = R(state, 30);
        state.lr = 0x82b87c00u;
        if (!crt_stream_close_error::Apply(0x82b87a38u, memory,
            dependencies, state))
            throw std::logic_error("missing recovered close callee");
        memory.WriteU32(Address(R(state, 31) + 80u), Address(R(state, 3)));
    }
    R(state, 12) = R(state, 31) + 144u;
    state.lr = 0x82b87c28u;
    Unlock(memory, dependencies, state);
    R(state, 3) = memory.ReadU32(Address(R(state, 31) + 80u));
    LeaveCaller(memory, state);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (address)
    {
    case kCaller: Caller(memory, dependencies, state); return true;
    case kUnlock: Unlock(memory, dependencies, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_close_caller
