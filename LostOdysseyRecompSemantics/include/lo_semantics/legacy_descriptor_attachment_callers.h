#pragma once

#include "lo_semantics/legacy_descriptor_value_intern.h"

namespace lo::semantic::gpu::legacy_descriptor_attachment_callers
{
using Registers = legacy_descriptor_value_intern::Registers;
using Services = legacy_descriptor_array_allocation::Services;

// Complete 83057B90 and 83057FB0 descriptor attachment callers. The
// accepted allocation, attachment copy/link and value intern bodies compose
// directly; pool exhaustion remains the accepted mutable native boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Services& services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_attachment_callers
