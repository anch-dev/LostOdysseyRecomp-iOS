#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_index_unlock_frames_context
{
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Exact indexed-unlock local frames. Native volatile/fault/MMIO/runtime behavior is bounded by the accepted selected lower service.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_index_unlock_frames_context
