#include "lo_semantics/crt_stream_close_error.h"

#include "lo_semantics/crt_status_error.h"
#include "lo_semantics/recovery_abi.h"

#include <stdexcept>

namespace lo::semantic::gpu::crt_stream_close_error
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

constexpr GuestAddress kClose = 0x82b87a38u;
constexpr GuestAddress kCallHandle = 0x82be1b80u;
constexpr GuestAddress kReleaseHandle = 0x82b86190u;
constexpr GuestAddress kBlockCount = 0x83378d68u;
constexpr GuestAddress kDispatchTable = 0x831e7df4u;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.r[index]; }

void Compare(crt_stream_operations::Condition& condition,
    std::uint64_t left, std::uint64_t right, std::uint8_t so, bool signed_words)
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

void Push(GuestMemory& memory, Registers& state, unsigned size)
{
    memory.WriteU32(Address(state.sp - size), Address(state.sp));
    state.sp -= size;
}

void EnterSimple(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    Push(memory, state, 96u);
}

void LeaveSimple(GuestMemory& memory, Registers& state)
{
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void EnterClose(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    for (unsigned index = 29; index <= 31; ++index)
        WriteU64(memory, Address(state.sp - 16u - 8u * (31u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    state.lr = 0x82b87a40u;
    Push(memory, state, 112u);
}

void LeaveClose(GuestMemory& memory, Registers& state)
{
    state.sp += 112u;
    for (unsigned index = 29; index <= 31; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 16u - 8u * (31u - index)));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void Accepted(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    if (!crt_stream_operations::ApplyAcceptedCallee(address, memory,
        dependencies.accepted, state))
        throw std::logic_error("missing accepted CRT stream callee");
}

void ConvertStatus(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    crt_status_error::Registers lower{};
    lower.sp = state.sp; lower.lr = state.lr;
    lower.r3 = R(state, 3); lower.r11 = R(state, 11);
    lower.r12 = R(state, 12); lower.r13 = R(state, 13);
    lower.xer_so = state.xer_so;
    lower.cr6 = {state.cr6.lt, state.cr6.gt, state.cr6.eq, state.cr6.so};
    if (!crt_status_error::Apply(0x827ca628u, memory,
        dependencies.accepted.io, lower))
        throw std::logic_error("missing accepted CRT status callee");
    state.sp = lower.sp; state.lr = lower.lr;
    R(state, 3) = lower.r3; R(state, 11) = lower.r11;
    R(state, 12) = lower.r12; R(state, 13) = lower.r13;
    state.cr6 = {lower.cr6.lt, lower.cr6.gt,
        lower.cr6.eq, lower.cr6.so};
}

void CallHandle(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    EnterSimple(memory, state);
    R(state, 11) = 0xffffffff831e0000ull;
    R(state, 11) = memory.ReadU32(kDispatchTable);
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + 4u));
    state.ctr = R(state, 11);
    state.lr = 0x82be1ba0u;
    dependencies.guest.CallIndirect(memory, Address(state.ctr) & ~3u, state);
    Compare(state.cr0, R(state, 3), 0, state.xer_so, true);
    if (state.cr0.lt)
    {
        state.lr = 0x82be1bb4u;
        ConvertStatus(memory, dependencies, state);
        R(state, 3) = 0;
    }
    else
        R(state, 3) = 1;
    LeaveSimple(memory, state);
}

void ReleaseHandle(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    EnterSimple(memory, state);
    Compare(state.cr6, R(state, 3), 0, state.xer_so, true);
    bool valid = !state.cr6.lt;
    if (valid)
    {
        R(state, 11) = 0xffffffff83380000ull;
        R(state, 11) = memory.ReadU32(kBlockCount);
        Compare(state.cr6, R(state, 3), R(state, 11), state.xer_so, false);
        valid = state.cr6.lt;
    }
    if (valid)
    {
        const auto index = R(state, 3);
        state.xer_ca = (static_cast<std::int32_t>(Address(index)) < 0) &&
            ((Address(index) & 31u) != 0);
        R(state, 10) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int32_t>(Address(index)) >> 5));
        R(state, 11) = 0xffffffff83380000ull;
        R(state, 9) = WordRotateMask(R(state, 10), 2, 0xfffffffcu);
        R(state, 11) -= 29312u;
        R(state, 10) = WordRotateMask(index, 6, 0x7c0u);
        R(state, 11) = memory.ReadU32(Address(R(state, 9) + R(state, 11)));
        R(state, 11) += R(state, 10);
        R(state, 10) = memory.ReadU8(Address(R(state, 11) + 4u));
        R(state, 10) &= 1u;
        Compare(state.cr0, R(state, 10), 0, state.xer_so, true);
        valid = !state.cr0.eq;
        if (valid)
        {
            R(state, 10) = memory.ReadU32(Address(R(state, 11)));
            Compare(state.cr6, R(state, 10), UINT32_MAX, state.xer_so, true);
            valid = !state.cr6.eq;
        }
    }
    if (valid)
    {
        R(state, 10) = UINT64_MAX;
        R(state, 3) = 0;
    }
    else
    {
        Accepted(0x82b7fd78u, memory, dependencies, state, 0x82b861f8u);
        R(state, 11) = 9;
        memory.WriteU32(Address(R(state, 3)), 9u);
        Accepted(0x82b7fdb0u, memory, dependencies, state, 0x82b86204u);
        R(state, 11) = R(state, 3);
        R(state, 10) = 0;
        R(state, 3) = UINT64_MAX;
    }
    memory.WriteU32(Address(R(state, 11)), Address(R(state, 10)));
    LeaveSimple(memory, state);
}

void Close(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    EnterClose(memory, state);
    R(state, 31) = R(state, 3);
    Accepted(0x82b86228u, memory, dependencies, state, 0x82b87a4cu);
    R(state, 11) = 0xffffffff83380000ull;
    Compare(state.cr6, R(state, 3), UINT32_MAX, state.xer_so, true);
    R(state, 29) = R(state, 11) - 29312u;
    bool call_handle = !state.cr6.eq;
    if (call_handle)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 29)));
        Compare(state.cr6, R(state, 31), 1u, state.xer_so, true);
        bool paired = false;
        if (state.cr6.eq)
        {
            R(state, 10) = memory.ReadU8(Address(R(state, 11) + 132u));
            R(state, 10) &= 1u;
            Compare(state.cr0, R(state, 10), 0, state.xer_so, true);
            paired = !state.cr0.eq;
        }
        if (!paired)
        {
            Compare(state.cr6, R(state, 31), 2u, state.xer_so, true);
            if (state.cr6.eq)
            {
                R(state, 11) = memory.ReadU8(Address(R(state, 11) + 68u));
                R(state, 11) &= 1u;
                Compare(state.cr0, R(state, 11), 0, state.xer_so, true);
                paired = !state.cr0.eq;
            }
        }
        if (paired)
        {
            R(state, 3) = 2;
            Accepted(0x82b86228u, memory, dependencies, state, 0x82b87a90u);
            R(state, 30) = R(state, 3);
            R(state, 3) = 1;
            Accepted(0x82b86228u, memory, dependencies, state, 0x82b87a9cu);
            Compare(state.cr6, R(state, 3), R(state, 30), state.xer_so, true);
            call_handle = !state.cr6.eq;
        }
    }
    if (call_handle)
    {
        R(state, 3) = R(state, 31);
        Accepted(0x82b86228u, memory, dependencies, state, 0x82b87aacu);
        state.lr = 0x82b87ab0u;
        CallHandle(memory, dependencies, state);
        Compare(state.cr0, R(state, 3), 0, state.xer_so, true);
        if (state.cr0.eq)
        {
            Accepted(0x822ca100u, memory, dependencies, state, 0x82b87abcu);
            R(state, 30) = R(state, 3);
        }
        else
            R(state, 30) = 0;
    }
    else
        R(state, 30) = 0;
    R(state, 3) = R(state, 31);
    state.lr = 0x82b87ad0u;
    ReleaseHandle(memory, dependencies, state);
    const auto handle = R(state, 31);
    state.xer_ca = (static_cast<std::int32_t>(Address(handle)) < 0) &&
        ((Address(handle) & 31u) != 0);
    R(state, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(Address(handle)) >> 5));
    R(state, 10) = WordRotateMask(handle, 6, 0x7c0u);
    R(state, 11) = WordRotateMask(R(state, 11), 2, 0xfffffffcu);
    Compare(state.cr6, R(state, 30), 0, state.xer_so, false);
    R(state, 11) = memory.ReadU32(Address(R(state, 11) + R(state, 29)));
    R(state, 11) += R(state, 10);
    R(state, 10) = 0;
    memory.WriteU8(Address(R(state, 11) + 4u), 0);
    if (!state.cr6.eq)
    {
        R(state, 3) = R(state, 30);
        Accepted(0x82b7fde8u, memory, dependencies, state, 0x82b87afcu);
        R(state, 3) = UINT64_MAX;
    }
    else
        R(state, 3) = 0;
    LeaveClose(memory, state);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (address)
    {
    case kClose: Close(memory, dependencies, state); return true;
    case kCallHandle: CallHandle(memory, dependencies, state); return true;
    case kReleaseHandle: ReleaseHandle(memory, dependencies, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_close_error
