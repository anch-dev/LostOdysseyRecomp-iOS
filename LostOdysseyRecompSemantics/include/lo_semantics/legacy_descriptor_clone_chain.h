#pragma once

#include "lo_semantics/legacy_descriptor_array_allocation.h"

namespace lo::semantic::gpu::legacy_descriptor_clone_chain
{
using Registers = legacy_descriptor_array_allocation::Registers;
using Services = legacy_descriptor_array_allocation::Services;

// Complete selected-context descriptor copy/attachment and four constructors,
// composing the accepted selector, pool allocator and mutation bodies.
// Exhausted-pool native implementation, other PPCContext fields, faults,
// concurrent list mutation, MMIO and runtime remain outside this interface.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Services& services, Registers& registers);
}
