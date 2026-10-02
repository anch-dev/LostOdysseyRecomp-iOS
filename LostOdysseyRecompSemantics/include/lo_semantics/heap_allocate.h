#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// The kernel calls and 827CC428 heap growth remain explicit guest boundaries.
// Virtual-memory output arguments point into the original guest stack frame.
class HeapAllocateServices
{
public:
    virtual ~HeapAllocateServices() = default;
    virtual std::uint32_t GetCurrentProcessType() = 0;
    virtual void BugCheck(std::uint32_t code, GuestAddress heap,
        GuestAddress caller_return, std::uint32_t line,
        std::uint32_t requested_bytes) = 0;
    virtual void EnterCriticalSection(GuestAddress address) = 0;
    virtual void LeaveCriticalSection(GuestAddress address) = 0;
    virtual GuestAddress GrowHeap(GuestAddress heap,
        std::uint32_t rounded_bytes) = 0;
    virtual std::int32_t AllocateVirtualMemory(GuestAddress base_inout,
        GuestAddress size_inout, std::uint32_t type,
        std::uint32_t protect, std::uint32_t zero) = 0;
    virtual void RaiseException(GuestAddress record_address) = 0;
};

// Observable cleanup of 823AD544; heap and lock_owned are the live r27/r22
// values, rather than reloads of their guest-frame mirrors.
void LeaveHeapAllocateCriticalSection(GuestMemory& memory,
    HeapAllocateServices& services, GuestAddress heap, std::uint32_t lock_owned);

// 823ACCB0. frame_base is its 320-byte guest frame; the ABI adapter supplies
// saved caller LR at frame_base + 312. Guest locals and VM out-parameters stay
// in that frame. Guest exception unwinding is outside this semantic function.
[[nodiscard]] GuestAddress AllocateHeapBlock(GuestMemory& memory,
    HeapAllocateServices& services, GuestAddress heap, std::uint32_t flags,
    std::uint32_t requested_bytes, GuestAddress frame_base);

} // namespace lo::semantic::gpu
