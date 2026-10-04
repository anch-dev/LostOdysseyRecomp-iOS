#pragma once

#include "lo_semantics/heap_insert_context.h"

namespace lo::semantic::gpu::heap_decommit_context
{
using Registers = heap_insert_context::Registers;

class BoundaryServices
{
public:
    virtual ~BoundaryServices() = default;
    // 827CB498 and 827CB658 retain live selected guest state and memory.
    virtual void CallDirect(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
    // NtFreeVirtualMemory retains live registers and may mutate guest RAM.
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Complete 827CC668 selected-context flow, including real 827CBA60
// free-list insertion. The two other guest helpers and kernel import are
// explicit mutable boundaries; other CR fields, faults, unwind, MMIO and
// concurrent heap mutation remain outside this selected-state interface.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_decommit_context
