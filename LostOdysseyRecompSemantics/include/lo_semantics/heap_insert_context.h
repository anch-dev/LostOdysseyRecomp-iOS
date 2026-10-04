#pragma once

#include "lo_semantics/raw_allocation_context.h"

namespace lo::semantic::gpu::heap_insert_context
{
using Registers = raw_allocation_context::Registers;

// Complete 827CBA60 selected-context free-list insertion: all 32 GPRs,
// CR0/CR6, XER SO/CA, LR/CTR and guest RAM. No direct or native callees.
// Other CR fields, faults, MMIO and concurrent heap mutation remain outside
// this selected-state interface.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& state);
} // namespace lo::semantic::gpu::heap_insert_context
