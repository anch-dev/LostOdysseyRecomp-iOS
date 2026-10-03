#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::record_buffer_extent
{
using Registers = crt_stream_operations::Registers;

// Select a fixed or count-derived extent while preserving the live PPC state.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
}
