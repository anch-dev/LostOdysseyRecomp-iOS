#pragma once

#include "lo_semantics/heap_range_context.h"

namespace lo::semantic::gpu::heap_growth_lower_context
{
using Registers = heap_range_context::Registers;

class BoundaryServices : public heap_range_context::BoundaryServices
{
public:
    // 827CB778 may call a heap-supplied indirect guest allocator. The
    // selected target address and live context remain explicit here.
    virtual void CallIndirect(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Complete selected-context 827CB778 commitment and 827CC2C0 segment
// creation; 827CB658 and 827CBA60 run selected implementations. Kernel VM
// imports and the configurable indirect call retain mutable boundaries.
// Only selected GPR/CR0/CR6/XER/LR/CTR/RAM state is represented.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_growth_lower_context
