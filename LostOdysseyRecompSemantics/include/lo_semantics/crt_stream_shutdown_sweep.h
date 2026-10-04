#pragma once

#include "lo_semantics/crt_stream_bulk_close_routes.h"

namespace lo::semantic::gpu::crt_stream_shutdown_sweep
{
using Registers = crt_stream_operations::Registers;
using Dependencies = crt_stream_bulk_close_routes::Dependencies;

// 82B817E8 walks the live stream table from index three, closes active
// records, releases entries at index twenty or above, and returns the number
// of successful closes. 82B818A8 is its independent indexed-unlock funclet.
// Accepted callees keep their documented selected guest/native boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_shutdown_sweep
