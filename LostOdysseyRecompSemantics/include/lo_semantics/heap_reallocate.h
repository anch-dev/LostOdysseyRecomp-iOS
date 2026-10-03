#pragma once

#include "lo_semantics/heap_allocate.h"
#include "lo_semantics/heap_block_resize.h"
#include "lo_semantics/heap_free.h"
#include "lo_semantics/heap_lock_exit.h"
#include "lo_semantics/heap_segment.h"

#include <cstdint>

namespace lo::semantic::gpu::heap_reallocate
{
struct Condition
{
    std::uint8_t lt = 0;
    std::uint8_t gt = 0;
    std::uint8_t eq = 0;
    std::uint8_t so = 0;
};

// Selected live PPC registers at the 827CCF80 boundary. The parent owns the
// 320-byte frame and save19/restore19 slots; all fields retain their full
// 64-bit value until an instruction explicitly loads a narrower value.
struct Registers
{
    std::uint64_t sp = 0;
    std::uint64_t lr = 0;
    std::uint64_t ctr = 0;
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    std::uint64_t r8 = 0, r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0;
    std::uint64_t r19 = 0, r20 = 0, r21 = 0, r22 = 0, r23 = 0;
    std::uint64_t r24 = 0, r25 = 0, r26 = 0, r27 = 0, r28 = 0;
    std::uint64_t r29 = 0, r30 = 0, r31 = 0;
    std::uint8_t xer_so = 0;
    Condition cr0{}, cr6{};
};

// The named lower helpers are called through their recovered C++ models.
// Native imports retain their actual call-site register/RAM boundary.
class Services
{
public:
    virtual ~Services() = default;
    virtual HeapAllocateServices& Allocation() = 0;
    virtual HeapFreeServices& Free() = 0;
    virtual HeapSegmentServices& Segment() = 0;
    virtual heap_block_resize::NativeServices& ResizeNative() = 0;
    virtual heap_lock_exit::NativeServices& LockExitNative() = 0;
    virtual void GetCurrentProcessType(GuestMemory& memory, Registers& registers) = 0;
    virtual void BugCheck(GuestMemory& memory, Registers& registers) = 0;
    virtual void EnterCriticalSection(GuestMemory& memory, Registers& registers) = 0;
    virtual void FreeVirtualMemory(GuestMemory& memory, Registers& registers) = 0;
    virtual void CompareMemoryUlong(GuestMemory& memory, Registers& registers) = 0;
    virtual void RaiseException(GuestMemory& memory, Registers& registers) = 0;
};

// Returns false without effects for an unknown address. A native bugcheck or
// exception that does not return propagates through its service; guest
// exception unwinding, faults, MMIO and concurrent mutation are not modeled.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Services& services, Registers& registers);
} // namespace lo::semantic::gpu::heap_reallocate
