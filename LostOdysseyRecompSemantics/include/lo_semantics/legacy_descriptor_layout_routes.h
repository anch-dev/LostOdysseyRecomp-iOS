#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_descriptor_layout_routes
{
using Registers = crt_stream_operations::Registers;

// Complete descriptor type classification and the 83054648 layout/flag path.
// The standard savegprlr_25 frame is modeled in ordinary guest RAM.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_layout_routes
