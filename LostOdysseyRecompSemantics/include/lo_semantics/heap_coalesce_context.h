#pragma once

#include "lo_semantics/heap_free_context.h"

namespace lo::semantic::gpu::heap_coalesce_context
{
using Registers = heap_free_context::Registers;
using BoundaryServices = heap_free_context::BoundaryServices;

// Complete 823AE108 selected integer/CR0/CR6/XER/LR/CTR/stack flow. The
// RtlCompareMemoryUlong import remains a mutable native boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);

// Validation composition: 823ADE28/823AE0BC call this selected coalescer;
// other guest and native boundaries are forwarded without reinterpretation.
[[nodiscard]] bool ApplyFree(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_coalesce_context
