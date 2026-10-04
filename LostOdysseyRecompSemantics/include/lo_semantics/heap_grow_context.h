#pragma once

#include "lo_semantics/heap_coalesce_context.h"

namespace lo::semantic::gpu::heap_grow_context
{
using Registers = heap_coalesce_context::Registers;

// 827CB778 and 827CC2C0 retain mutable selected guest state until their
// own selected-context implementations are composed.
using BoundaryServices = heap_coalesce_context::BoundaryServices;

// Complete 827CC428 selected integer/CR0/CR6/XER/LR/CTR/stack flow.
// Accepted coalescing and free-list insertion run as selected callees;
// 827CB778/827CC2C0 and kernel VM imports remain mutable boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_grow_context
