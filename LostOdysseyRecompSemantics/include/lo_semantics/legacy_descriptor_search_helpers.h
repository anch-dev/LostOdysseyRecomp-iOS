#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_descriptor_search_helpers
{
using Registers = crt_stream_operations::Registers;

// Complete descriptor type/flag predicates, two-bit normalization, and
// linked interval overlap with its 83057B58 fallback walk.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_search_helpers
