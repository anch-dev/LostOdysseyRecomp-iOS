#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::heap_lock_exit
{
struct Registers
{
    std::uint64_t sp = 0;
    std::uint64_t lr = 0;
    std::uint64_t r3 = 0;
    std::uint64_t r11 = 0;
    std::uint64_t r12 = 0;
    std::uint64_t r22 = 0;
    std::uint64_t r31 = 0;
    std::uint8_t xer_so = 0;
    struct Condition { std::uint8_t lt = 0, gt = 0, eq = 0, so = 0; } cr6;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // Original __imp__RtlLeaveCriticalSection at 0x830D9C7C. The callback
    // receives live selected registers and RAM after bl has set LR. It may
    // redirect SP or alter saved slots before the original epilogue reloads.
    virtual void LeaveCriticalSection(GuestMemory& memory,
        Registers& registers) = 0;
};

// Complete 827CD7BC control flow on ordinary RAM and selected PPC state.
// Unknown addresses have no effects. Unselected native volatile registers,
// faults, MMIO and concurrency remain outside this interface.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& registers);
} // namespace lo::semantic::gpu::heap_lock_exit
