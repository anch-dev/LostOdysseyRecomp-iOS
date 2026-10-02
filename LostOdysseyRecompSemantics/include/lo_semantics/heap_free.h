#pragma once

#include "lo_semantics/heap.h"

namespace lo::semantic::gpu
{

// Unrecovered kernel calls and 827CC668 remain explicit boundaries. The VM
// output arguments are guest addresses into the caller-provided frame.
class HeapFreeServices : public HeapServices
{
public:
    virtual std::uint32_t GetCurrentProcessType() = 0;
    virtual void BugCheck(std::uint32_t code, GuestAddress heap,
        GuestAddress caller_return, std::uint32_t line, GuestAddress payload) = 0;
    virtual void EnterCriticalSection(GuestAddress address) = 0;
    virtual void LeaveCriticalSection(GuestAddress address) = 0;
    virtual std::int32_t FreeVirtualMemory(GuestAddress base_out,
        GuestAddress size_out, std::uint32_t type, std::uint32_t zero) = 0;
    virtual void DecommitFreeBlock(GuestAddress heap, GuestAddress block,
        std::uint32_t units) = 0;
};

// 823AE0BC's observable cleanup: r30 is the heap and r25 is lock_owned.
// Guest exception landing pad 823AE094 and ABI register restoration are not
// modeled by this ordinary C++ function.
void LeaveHeapCriticalSection(GuestMemory& memory, HeapFreeServices& services,
    GuestAddress heap, std::uint32_t lock_owned);

// 823ADE28. frame_base names the original 176-byte guest frame. The ABI
// adapter or fixture supplies saved caller LR at frame_base + 168. Stack
// locals are read and written in GuestMemory, including VM output pointers.
[[nodiscard]] std::uint32_t FreeHeapBlock(GuestMemory& memory,
    HeapFreeServices& services, GuestAddress heap, std::uint32_t flags,
    GuestAddress payload, GuestAddress frame_base);

} // namespace lo::semantic::gpu
