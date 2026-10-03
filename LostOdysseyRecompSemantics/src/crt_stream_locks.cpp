#include "lo_semantics/crt_stream_locks.h"

#include "lo_semantics/allocation_failure.h"
#include "lo_semantics/memory_services.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_stream_locks
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

constexpr GuestAddress kLockTable = 0x83215358u;
constexpr GuestAddress kStreamBlocks = 0x83378d80u;
constexpr GuestAddress kFatalTarget = 0x83214d70u;

void CompareSigned(Condition& condition, std::uint64_t value,
    std::uint8_t summary_overflow)
{
    const auto word = static_cast<std::int32_t>(value);
    condition = {word < 0, word > 0, word == 0, summary_overflow != 0};
}

void CompareUnsigned(Condition& condition, std::uint64_t value,
    std::uint8_t summary_overflow)
{
    condition = {false, static_cast<std::uint32_t>(value) != 0,
        static_cast<std::uint32_t>(value) == 0, summary_overflow != 0};
}

void SaveNonvolatile(GuestMemory& memory, Registers& state, unsigned first)
{
    const auto sp = Address(state.sp);
    // These offsets and the store order are the actual __savegprlr_28/_29
    // helper bodies at ppc_recomp.175.cpp:5671 and :5691.
    if (first == 28) WriteU64(memory, sp - 40u, state.r28);
    WriteU64(memory, sp - 32u, state.r29);
    WriteU64(memory, sp - 24u, state.r30);
    WriteU64(memory, sp - 16u, state.r31);
    memory.WriteU32(sp - 8u, Address(state.r12));
}

void RestoreNonvolatile(GuestMemory& memory, Registers& state, unsigned first)
{
    const auto sp = Address(state.sp);
    // Load order likewise follows __restgprlr_28/_29 at :6257 and :6279.
    if (first == 28) state.r28 = ReadU64(memory, sp - 40u);
    state.r29 = ReadU64(memory, sp - 32u);
    state.r30 = ReadU64(memory, sp - 24u);
    state.r31 = ReadU64(memory, sp - 16u);
    state.r12 = memory.ReadU32(sp - 8u);
    state.lr = state.r12;
}

void PushFrame(GuestMemory& memory, Registers& state, unsigned bytes)
{
    const auto previous_sp = Address(state.sp);
    state.sp -= bytes;
    memory.WriteU32(Address(state.sp), previous_sp);
}

void CallInitialization(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    InvalidParameterCall call{{{state.r3, state.r4, state.r5, state.r6,
        state.r7, state.r8, state.r9, state.r10}}, state.r13};
    crt_stream_state::FrameRegisters frame{state.lr, state.r31, state.sp};
    std::uint64_t result = 0;
    (void)crt_stream_state::Apply(0x82b821b0u, memory,
        dependencies.thread, dependencies.invalid, dependencies.raw,
        dependencies.initialization, call, state.sp, frame, result);
    state.r3 = result;
    state.r4 = call.arguments[1]; state.r5 = call.arguments[2];
    state.r6 = call.arguments[3]; state.r7 = call.arguments[4];
    state.r8 = call.arguments[5]; state.r9 = call.arguments[6];
    state.r10 = call.arguments[7]; state.r13 = call.thread_environment;
    state.sp = frame.sp; state.lr = frame.lr; state.r31 = frame.r31;
}

void UnlockGlobal(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    crt_stream_pointer_unlock::Registers call{state.sp, state.lr, state.r3,
        state.r9, state.r10, state.r11, state.r12, state.r30,
        state.r31, state.xer_ca};
    (void)crt_stream_pointer_unlock::Apply(0x82b81af8u, memory,
        dependencies.pointer_unlock, call);
    state.sp = call.sp; state.lr = call.lr; state.r3 = call.r3;
    state.r9 = call.r9; state.r10 = call.r10; state.r11 = call.r11;
    state.r12 = call.r12; state.r30 = call.r30; state.r31 = call.r31;
    state.xer_ca = call.xer_ca;
}

void UnlockIndex(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    crt_stream_index_unlock::Registers call{state.sp, state.lr, state.r3,
        state.r10, state.r11, state.r12, state.r29, state.r31};
    (void)crt_stream_index_unlock::Apply(0x82b863b8u, memory,
        dependencies.index_unlock, call);
    state.sp = call.sp; state.lr = call.lr; state.r3 = call.r3;
    state.r10 = call.r10; state.r11 = call.r11; state.r12 = call.r12;
    state.r29 = call.r29; state.r31 = call.r31;
}

void FatalRuntimeError(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    WriteU64(memory, Address(state.sp - 16u), state.r31);
    PushFrame(memory, state, 96);
    state.r31 = state.r3;
    state.lr = 0x82b7bef0u;
    state.r3 = ReportMissingHeapBanner(memory, dependencies.allocation);
    state.r3 = state.r31;
    state.lr = 0x82b7bef8u;
    state.r3 = ReportRuntimeError(memory, dependencies.allocation, state.r3);
    state.r11 = 0xffffffff83210000ull;
    state.r3 = 255;
    state.r11 = memory.ReadU32(kFatalTarget);
    state.ctr = state.r11;
    state.lr = 0x82b7bf0cu;
    dependencies.native.CallFatal(Address(state.ctr) & ~3u, memory, state);
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    state.r31 = ReadU64(memory, Address(state.sp - 16u));
}

void AcquireGlobal(GuestMemory& memory, Dependencies dependencies,
    Registers& state);

void InitializeGlobal(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    state.r12 = state.lr;
    SaveNonvolatile(memory, state, 28);
    state.r31 = state.sp - 128u;
    PushFrame(memory, state, 128);
    state.r29 = state.r3;
    state.r11 = 1;
    memory.WriteU32(Address(state.r31 + 80u), 1);
    state.lr = 0x82b81a08u;
    state.r3 = GetProcessHeap(memory);
    CompareUnsigned(state.cr0, state.r3, state.xer_so);
    if (state.cr0.eq)
    {
        state.lr = 0x82b81a14u;
        state.r3 = ReportMissingHeapBanner(memory, dependencies.allocation);
        state.r3 = 30;
        state.lr = 0x82b81a1cu;
        state.r3 = ReportRuntimeError(memory, dependencies.allocation, state.r3);
        state.r3 = 255;
        state.lr = 0x82b81a24u;
        state.r3 = TerminateAllocationFailure(dependencies.allocation);
    }

    state.r30 = 0xffffffff00000000ull | kLockTable;
    state.r29 = WordRotateMask(state.r29, 3, 0xfffffff8u);
    state.r11 = memory.ReadU32(Address(state.r29 + state.r30));
    CompareUnsigned(state.cr6, state.r11, state.xer_so);
    if (state.cr6.eq)
    {
        state.r3 = 28;
        state.lr = 0x82b81a4cu;
        state.r3 = AllocateRawMemory(memory, dependencies.raw, state.r3);
        state.r28 = state.r3;
        CompareSigned(state.cr0, state.r28, state.xer_so);
        if (state.cr0.eq)
        {
            state.lr = 0x82b81a58u;
            state.r3 = GetAllocationErrorAddress(dependencies.allocation);
            state.r11 = state.r3;
            state.r10 = 12;
            state.r3 = 0;
            memory.WriteU32(Address(state.r11), 12);
        }
        else
        {
            state.r3 = 10;
            state.lr = 0x82b81a74u;
            AcquireGlobal(memory, dependencies, state);
            state.r11 = memory.ReadU32(Address(state.r29 + state.r30));
            CompareUnsigned(state.cr6, state.r11, state.xer_so);
            state.r3 = state.r28;
            if (state.cr6.eq)
            {
                state.r4 = 4000;
                state.lr = 0x82b81a90u;
                CallInitialization(memory, dependencies, state);
                CompareSigned(state.cr0, state.r3, state.xer_so);
                if (state.cr0.eq)
                {
                    state.r3 = state.r28;
                    state.lr = 0x82b81aa0u;
                    state.r3 = FreeCrtRecord(memory, dependencies.allocation,
                        state.r3, Address(state.r13), Address(state.sp));
                    state.lr = 0x82b81aa4u;
                    state.r3 = GetAllocationErrorAddress(dependencies.allocation);
                    state.r11 = 12;
                    memory.WriteU32(Address(state.r3), 12);
                    state.r11 = 0;
                    memory.WriteU32(Address(state.r31 + 80u), 0);
                }
                else
                {
                    memory.WriteU32(Address(state.r29 + state.r30),
                        Address(state.r28));
                }
            }
            else
            {
                state.lr = 0x82b81ac4u;
                state.r3 = FreeCrtRecord(memory, dependencies.allocation,
                    state.r3, Address(state.r13), Address(state.sp));
            }
            state.r12 = state.r31 + 128u;
            state.lr = 0x82b81ad0u;
            UnlockGlobal(memory, dependencies, state);
            state.r3 = memory.ReadU32(Address(state.r31 + 80u));
        }
    }
    else
    {
        state.r3 = 1;
    }
    state.sp = state.r31 + 128u;
    RestoreNonvolatile(memory, state, 28);
}

void AcquireGlobal(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    WriteU64(memory, Address(state.sp - 24u), state.r30);
    WriteU64(memory, Address(state.sp - 16u), state.r31);
    PushFrame(memory, state, 112);
    state.r11 = 0xffffffff83210000ull;
    state.r30 = WordRotateMask(state.r3, 3, 0xfffffff8u);
    state.r31 = state.r11 + 21336u;
    state.r11 = memory.ReadU32(Address(state.r30 + state.r31));
    CompareUnsigned(state.cr6, state.r11, state.xer_so);
    if (state.cr6.eq)
    {
        state.lr = 0x82b81b58u;
        InitializeGlobal(memory, dependencies, state);
        CompareSigned(state.cr0, state.r3, state.xer_so);
        if (state.cr0.eq)
        {
            state.r3 = 17;
            state.lr = 0x82b81b68u;
            FatalRuntimeError(memory, dependencies, state);
        }
    }
    state.r3 = memory.ReadU32(Address(state.r30 + state.r31));
    state.lr = 0x82b81b70u;
    dependencies.native.EnterCriticalSection(memory, state);
    state.sp += 112u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    state.r30 = ReadU64(memory, Address(state.sp - 24u));
    state.r31 = ReadU64(memory, Address(state.sp - 16u));
}

void AcquireStream(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    state.r12 = state.lr;
    SaveNonvolatile(memory, state, 29);
    state.r31 = state.sp - 128u;
    PushFrame(memory, state, 128);
    memory.WriteU32(Address(state.r31 + 148u), Address(state.r3));
    state.r11 = 0xffffffff00000000ull | kStreamBlocks;
    const auto index = static_cast<std::int32_t>(state.r3);
    state.xer_ca = static_cast<std::uint8_t>(index < 0 &&
        (static_cast<std::uint32_t>(index) & 31u) != 0);
    state.r10 = static_cast<std::uint64_t>(index >> 5);
    state.r10 = WordRotateMask(state.r10, 2, 0xfffffffcu);
    state.r9 = WordRotateMask(state.r3, 6, 0x7c0u);
    state.r29 = 1;
    memory.WriteU32(Address(state.r31 + 80u), 1);
    state.r10 = memory.ReadU32(Address(state.r10 + state.r11));
    state.r30 = state.r10 + state.r9;
    state.r10 = memory.ReadU32(Address(state.r30 + 8u));
    CompareSigned(state.cr6, state.r10, state.xer_so);
    if (state.cr6.eq)
    {
        state.r3 = 10;
        state.lr = 0x82b86344u;
        AcquireGlobal(memory, dependencies, state);
        state.r11 = memory.ReadU32(Address(state.r30 + 8u));
        CompareSigned(state.cr6, state.r11, state.xer_so);
        if (state.cr6.eq)
        {
            state.r4 = 4000;
            state.r3 = state.r30 + 12u;
            state.lr = 0x82b86360u;
            CallInitialization(memory, dependencies, state);
            CompareSigned(state.cr0, state.r3, state.xer_so);
            if (state.cr0.eq)
            {
                state.r11 = 0;
                memory.WriteU32(Address(state.r31 + 80u), 0);
            }
            state.r11 = memory.ReadU32(Address(state.r30 + 8u));
            state.r11 += 1u;
            memory.WriteU32(Address(state.r30 + 8u), Address(state.r11));
        }
        state.r12 = state.r31 + 128u;
        state.lr = 0x82b86388u;
        UnlockIndex(memory, dependencies, state);
    }
    CompareSigned(state.cr6, state.r29, state.xer_so);
    if (!state.cr6.eq)
    {
        const auto final_index = static_cast<std::int32_t>(state.r3);
        state.xer_ca = static_cast<std::uint8_t>(final_index < 0 &&
            (static_cast<std::uint32_t>(final_index) & 31u) != 0);
        state.r10 = static_cast<std::uint64_t>(final_index >> 5);
        state.r9 = WordRotateMask(state.r10, 2, 0xfffffffcu);
        state.r10 = WordRotateMask(state.r3, 6, 0x7c0u);
        state.r11 = memory.ReadU32(Address(state.r9 + state.r11));
        state.r11 += state.r10;
        state.r3 = state.r11 + 12u;
        state.lr = 0x82b863acu;
        dependencies.native.EnterCriticalSection(memory, state);
    }
    state.r3 = state.r29;
    state.sp = state.r31 + 128u;
    RestoreNonvolatile(memory, state, 29);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (address)
    {
    case 0x82b81b28u:
        AcquireGlobal(memory, dependencies, registers);
        return true;
    case 0x82b819e8u:
        InitializeGlobal(memory, dependencies, registers);
        return true;
    case 0x82b7bed8u:
        FatalRuntimeError(memory, dependencies, registers);
        return true;
    case 0x82b862f8u:
        AcquireStream(memory, dependencies, registers);
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_locks
