#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_stream_scan_adjacent_context
{
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Exact 82DF5D18 bounded string append and 82DF5E30 integer formatting
// bodies. Their direct CRT errno callees use the accepted selected-context
// implementation; faults, MMIO and concurrent mutation remain outside scope.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_scan_adjacent_context
