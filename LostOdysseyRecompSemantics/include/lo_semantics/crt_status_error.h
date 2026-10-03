#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::crt_status_error
{
struct Condition { std::uint8_t lt = 0, gt = 0, eq = 0, so = 0; };
struct Registers
{
    std::uint64_t sp = 0, lr = 0;
    std::uint64_t r3 = 0, r11 = 0, r12 = 0, r13 = 0;
    std::uint8_t xer_so = 0;
    Condition cr6;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // RtlNtStatusToDosError receives the full live r3 after the original bl.
    // Subsequent thread writes and the additive epilogue follow live r13/SP.
    virtual void NtStatusToDosError(GuestMemory& memory, Registers& state) = 0;
};

// 827CA628 converts status and conditionally writes the current thread error.
// Own frame, selected registers and ordinary RAM are modeled. Native internals,
// unselected volatile state, faults/MMIO, concurrency and runtime remain open.
// Unknown addresses return false without touching memory, services or state.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::crt_status_error
