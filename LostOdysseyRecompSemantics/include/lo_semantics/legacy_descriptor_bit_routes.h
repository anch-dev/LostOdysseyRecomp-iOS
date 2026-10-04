#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_descriptor_bit_routes
{
using Registers = crt_stream_operations::Registers;

// Complete 83053308 descriptor classification and 83054390 bit-field
// propagation, including the latter's real classifier and save/restore frame.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_bit_routes
