#include "lo_semantics/crt_stream_io.h"

#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/thread_state.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_stream_io
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;

std::uint64_t Sign32(std::uint32_t word)
{
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(
        static_cast<std::int32_t>(word)));
}

template <class T>
void Compare(Condition& condition, T left, T right, std::uint8_t so)
{
    condition = {static_cast<std::uint8_t>(left < right),
        static_cast<std::uint8_t>(left > right),
        static_cast<std::uint8_t>(left == right), so};
}

void Enter(GuestMemory& memory, Registers& state, unsigned frame,
    unsigned first_saved)
{
    state.r12 = state.lr;
    const auto caller = Address(state.sp);
    if (first_saved == 28)
        WriteU64(memory, caller - 40u, state.r28);
    WriteU64(memory, caller - 32u, state.r29);
    WriteU64(memory, caller - 24u, state.r30);
    WriteU64(memory, caller - 16u, state.r31);
    memory.WriteU32(caller - 8u, Address(state.r12));
    const auto next_sp = state.sp - frame;
    memory.WriteU32(Address(next_sp), caller);
    state.sp = next_sp;
}

void Leave(GuestMemory& memory, Registers& state, unsigned frame,
    unsigned first_saved)
{
    state.sp += frame;
    const auto caller = Address(state.sp);
    if (first_saved == 28)
        state.r28 = ReadU64(memory, caller - 40u);
    state.r29 = ReadU64(memory, caller - 32u);
    state.r30 = ReadU64(memory, caller - 24u);
    state.r31 = ReadU64(memory, caller - 16u);
    state.r12 = memory.ReadU32(caller - 8u);
    state.lr = state.r12;
}

void ConvertStatus(GuestMemory& memory, NativeServices& native,
    Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    crt_status_error::Registers lower{state.sp, state.lr, state.r3,
        state.r11, state.r12, state.r13, state.xer_so, state.cr6};
    (void)crt_status_error::Apply(0x827ca628u, memory, native, lower);
    state.sp = lower.sp;
    state.lr = lower.lr;
    state.r3 = lower.r3;
    state.r11 = lower.r11;
    state.r12 = lower.r12;
    state.r13 = lower.r13;
    state.xer_so = lower.xer_so;
    state.cr6 = lower.cr6;
}

void StoreFailure(GuestMemory& memory, Registers& state,
    std::uint32_t code, bool allocation)
{
    state.r3 = code;
    state.lr = allocation ? 0x82be2a80u : 0x82be2a10u;
    // The tail at 822CA180 enters the same tiny RAM algorithm as 822CA188.
    state.r11 = memory.ReadU32(Address(state.r13 + 336u));
    Compare(state.cr6, Address(state.r11), std::uint32_t{0}, state.xer_so);
    if (state.cr6.eq)
        state.r11 = memory.ReadU32(Address(state.r13 + 256u));
    if (allocation)
        ReportAllocationFailure(memory, Address(state.r13), code);
    else
        StoreThreadFailureCode(memory, Address(state.r13), code);
}

void CallIndirect(GuestMemory& memory, NativeServices& native,
    Registers& state, GuestAddress slot, std::uint64_t length,
    std::uint64_t flag, unsigned local, GuestAddress return_address)
{
    state.r11 = memory.ReadU32(Address(state.r31 + 32244u));
    state.r7 = flag;
    state.r6 = length;
    state.r5 = state.sp + local;
    state.r4 = state.sp + 88u;
    state.r3 = state.r29;
    state.r11 = memory.ReadU32(Address(state.r11 + slot));
    state.ctr = state.r11;
    state.lr = return_address;
    native.CallIndirect(memory, Address(state.ctr) & ~3u, state);
}

void WriteFile(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    Enter(memory, state, 128, 29);
    state.lr = 0x82be2818u;
    state.r29 = state.r6; // caller's byte-count output
    state.r30 = state.r3; // file handle
    state.r8 = state.r4;  // buffer
    state.r9 = state.r5;  // byte length
    state.r31 = state.r7; // optional I/O status block
    state.r6 = 0;
    Compare(state.cr6, Address(state.r29), std::uint32_t{0}, state.xer_so);
    if (Address(state.r29) != 0)
        memory.WriteU32(Address(state.r29), 0);
    Compare(state.cr6, Address(state.r31), std::uint32_t{0}, state.xer_so);

    if (Address(state.r31) != 0)
    {
        state.r4 = memory.ReadU32(Address(state.r31 + 16u));
        state.r11 = 259;
        state.r10 = memory.ReadU32(Address(state.r31 + 8u));
        memory.WriteU32(Address(state.r31), 259);
        memory.WriteU32(Address(state.sp + 92u), Address(state.r10));
        state.r10 = memory.ReadU32(Address(state.r31 + 12u));
        memory.WriteU32(Address(state.sp + 88u), Address(state.r10));
        state.r11 = Address(state.r4) & 1u;
        Compare(state.cr0, static_cast<std::int32_t>(Address(state.r11)),
            std::int32_t{0}, state.xer_so);
        if (state.cr0.eq) state.r6 = state.r31;
        state.r10 = state.sp + 88u;
        state.r7 = state.r31;
        state.r5 = 0;
        state.r3 = state.r30;
        state.lr = 0x82be2884u;
        native.NtWriteFile(memory, state);
        state.r11 = Sign32(0xc0000000u);
        state.r10 = WordRotateMask(state.r3, 0, 0xc0000000u);
        Compare(state.cr6, Address(state.r10), Address(state.r11), state.xer_so);
        bool failed = state.cr6.eq;
        if (!failed)
        {
            Compare(state.cr6, static_cast<std::int32_t>(Address(state.r3)),
                std::int32_t{259}, state.xer_so);
            failed = state.cr6.eq;
        }
        if (!failed)
        {
            Compare(state.cr6, Address(state.r29), std::uint32_t{0}, state.xer_so);
            if (!state.cr6.eq)
            {
                state.r11 = memory.ReadU32(Address(state.r31 + 4u));
                memory.WriteU32(Address(state.r29), Address(state.r11));
            }
            state.r3 = 1;
        }
        else
        {
            ConvertStatus(memory, native, state, 0x82be292cu);
            state.r3 = 0;
        }
    }
    else
    {
        state.r10 = 0;
        state.r7 = state.sp + 80u;
        state.r6 = state.r5 = state.r4 = 0;
        state.r3 = state.r30;
        state.lr = 0x82be28d0u;
        native.NtWriteFile(memory, state);
        Compare(state.cr6, static_cast<std::int32_t>(Address(state.r3)),
            std::int32_t{259}, state.xer_so);
        bool failed = false;
        if (state.cr6.eq)
        {
            state.r6 = state.r5 = 0;
            state.r4 = 1;
            state.r3 = state.r30;
            state.lr = 0x82be28ecu;
            native.NtWaitForSingleObjectEx(memory, state);
            Compare(state.cr0, static_cast<std::int32_t>(Address(state.r3)),
                std::int32_t{0}, state.xer_so);
            failed = state.cr0.lt;
            if (!failed)
                state.r3 = memory.ReadU32(Address(state.sp + 80u));
        }
        if (!failed)
        {
            Compare(state.cr6, static_cast<std::int32_t>(Address(state.r3)),
                std::int32_t{0}, state.xer_so);
            failed = state.cr6.lt;
        }
        if (!failed)
        {
            state.r11 = memory.ReadU32(Address(state.sp + 84u));
            state.r3 = 1;
            memory.WriteU32(Address(state.r29), Address(state.r11));
        }
        else
        {
            state.r11 = WordRotateMask(state.r3, 0, 0xc0000000u);
            state.r10 = Sign32(0x80000000u);
            Compare(state.cr6, Address(state.r11), Address(state.r10), state.xer_so);
            if (state.cr6.eq)
            {
                state.r11 = memory.ReadU32(Address(state.sp + 84u));
                memory.WriteU32(Address(state.r29), Address(state.r11));
            }
            ConvertStatus(memory, native, state, 0x82be292cu);
            state.r3 = 0;
        }
    }
    Leave(memory, state, 128, 29);
}

void SeekFilePosition(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    Enter(memory, state, 192, 28);
    state.lr = 0x82be2940u;
    state.r28 = state.r5; // optional high word of the 64-bit input/output position
    state.r29 = state.r3; // stream
    Compare(state.cr6, Address(state.r28), std::uint32_t{0}, state.xer_so);
    if (Address(state.r28) != 0)
    {
        state.r11 = memory.ReadU32(Address(state.r28));
        memory.WriteU32(Address(state.sp + 84u), Address(state.r4));
        memory.WriteU32(Address(state.sp + 80u), Address(state.r11));
        state.r30 = ReadU64(memory, Address(state.sp + 80u));
    }
    else
        state.r30 = Sign32(Address(state.r4));

    Compare(state.cr6, Address(state.r6), std::uint32_t{1}, state.xer_so);
    state.r31 = Sign32(0x831e0000u);
    if (Address(state.r6) >= 1u)
    {
        if (Address(state.r6) > 1u)
            Compare(state.cr6, Address(state.r6), std::uint32_t{3},
                state.xer_so);
        if (Address(state.r6) == 1u || Address(state.r6) >= 3u)
        {
            CallIndirect(memory, native, state, 32u, 8, 14, 80, 0x82be29e8u);
            Compare(state.cr0, static_cast<std::int32_t>(Address(state.r3)),
                std::int32_t{0}, state.xer_so);
            if (state.cr0.lt)
            {
                ConvertStatus(memory, native, state, 0x82be29b4u);
                goto failure;
            }
            state.r11 = ReadU64(memory, Address(state.sp + 80u));
        }
        else
        {
            CallIndirect(memory, native, state, 32u, 56, 34, 96, 0x82be29a8u);
            Compare(state.cr0, static_cast<std::int32_t>(Address(state.r3)),
                std::int32_t{0}, state.xer_so);
            if (state.cr0.lt)
            {
                ConvertStatus(memory, native, state, 0x82be29b4u);
                goto failure;
            }
            state.r11 = ReadU64(memory, Address(state.sp + 136u));
        }
        state.r11 += state.r30;
    }
    else
        state.r11 = state.r30;

    WriteU64(memory, Address(state.sp + 80u), state.r11);
    Compare(state.cr6, static_cast<std::int64_t>(state.r11),
        std::int64_t{0}, state.xer_so);
    if (state.cr6.lt)
    {
        StoreFailure(memory, state, 131, false);
        goto failure;
    }
    Compare(state.cr6, Address(state.r28), std::uint32_t{0}, state.xer_so);
    if (state.cr6.eq)
    {
        state.r11 = memory.ReadU32(Address(state.sp + 80u));
        state.r11 = Address(state.r11) & 0x7fffffffu;
        Compare(state.cr0, static_cast<std::int32_t>(Address(state.r11)),
            std::int32_t{0}, state.xer_so);
        if (!state.cr0.eq)
        {
            StoreFailure(memory, state, 87, false);
            goto failure;
        }
    }
    CallIndirect(memory, native, state, 36u, 8, 14, 80, 0x82be2a54u);
    Compare(state.cr0, static_cast<std::int32_t>(Address(state.r3)),
        std::int32_t{0}, state.xer_so);
    if (state.cr0.lt)
    {
        ConvertStatus(memory, native, state, 0x82be2a8cu);
        Compare(state.cr6, Address(state.r28), std::uint32_t{0}, state.xer_so);
        if (state.cr6.eq) goto failure;
        state.r11 = UINT64_MAX;
        memory.WriteU32(Address(state.r28), Address(state.r11));
        goto failure;
    }
    Compare(state.cr6, Address(state.r28), std::uint32_t{0}, state.xer_so);
    if (!state.cr6.eq)
    {
        state.r11 = memory.ReadU32(Address(state.sp + 80u));
        memory.WriteU32(Address(state.r28), Address(state.r11));
    }
    state.r11 = memory.ReadU32(Address(state.sp + 84u));
    Compare(state.cr6, static_cast<std::int32_t>(Address(state.r11)),
        std::int32_t{-1}, state.xer_so);
    if (state.cr6.eq)
        StoreFailure(memory, state, 0, true);
    state.r3 = memory.ReadU32(Address(state.sp + 84u));
    goto done;

failure:
    state.r3 = UINT64_MAX;
done:
    Leave(memory, state, 192, 28);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    switch (address)
    {
    case 0x82be2810u: WriteFile(memory, native, state); return true;
    case 0x82be2938u: SeekFilePosition(memory, native, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_io
