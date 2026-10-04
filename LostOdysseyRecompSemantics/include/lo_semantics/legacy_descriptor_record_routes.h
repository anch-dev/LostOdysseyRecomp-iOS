#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_descriptor_record_routes
{
using Registers = crt_stream_operations::Registers;

// Complete 830555E0 descriptor record construction and its 82FAC238
// indexed-record selector. The selector reads the original image jump table.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_record_routes
