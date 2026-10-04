#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_numeric61_context
{
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Exact numeric tail and locale byte-search bodies; accepted lower context composition.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_numeric61_context
