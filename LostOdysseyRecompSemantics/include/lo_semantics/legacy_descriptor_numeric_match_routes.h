#pragma once

#include "lo_semantics/legacy_descriptor_numeric_read.h"

namespace lo::semantic::gpu::legacy_descriptor_numeric_match_routes
{
using Registers=legacy_descriptor_numeric_read::Registers;
using Services=legacy_descriptor_numeric_read::Services;
// Numeric extraction with accepted scalar/flag bodies and complete descriptor
// equivalence predicate. The diagnostic selected boundary, exceptional numeric
// conversions, other context/layouts, faults, MMIO and runtime remain open.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,
    Services& services,Registers& registers);
}
