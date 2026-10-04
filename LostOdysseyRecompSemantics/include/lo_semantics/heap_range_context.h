#pragma once

#include "lo_semantics/heap_insert_context.h"

namespace lo::semantic::gpu::heap_range_context
{
using Registers = heap_insert_context::Registers;

class BoundaryServices
{
public:
    virtual ~BoundaryServices() = default;
    // NtAllocateVirtualMemory and NtFreeVirtualMemory retain live selected
    // registers and may change pointed-to guest arguments and memory.
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Complete selected-context flow for 827CB498 descriptor acquisition and
// 827CB658 free-range insertion/coalescing, including their direct call.
// Tracks all 32 GPR, CR0/CR6, XER SO/CA, LR/CTR and guest RAM. Other CR
// fields, faults, unwind, MMIO and concurrent heap mutation remain open.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_range_context
