#pragma once
#include "lo_semantics/crt_stream_operations.h"
namespace lo::semantic::gpu::crt_stream_counted_output
{
// Count one emitted byte, or mark the caller count on an output error.
// Lower stream implementations retain their recorded native boundaries.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&,
    crt_stream_operations::Dependencies, crt_stream_operations::Registers&);
}
