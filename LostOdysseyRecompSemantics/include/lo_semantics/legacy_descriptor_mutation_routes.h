#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_descriptor_mutation_routes
{
using Registers = crt_stream_operations::Registers;

// Complete 83056568 descriptor mutation and its 83056AC0 caller. Both call
// the accepted 82FAC238 indexed-record selector with live register state.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_mutation_routes
