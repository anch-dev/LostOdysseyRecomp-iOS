#pragma once

#include "lo_semantics/crt_record_allocation_context.h"
#include "lo_semantics/crt_stream_resize_context.h"

namespace lo::semantic::gpu::crt_stream_buffer_growth_callers
{
using Registers = crt_async_status_transfer::Registers;

struct Dependencies
{
    crt_record_allocation_context::Dependencies allocation;
    crt_stream_resize_context::Dependencies resize;
};

// Two complete buffer-growth callers. The element width changes the accepted
// record allocation, byte-copy length and resize request.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_buffer_growth_callers
