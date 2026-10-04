#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_numeric_parse_chain61_context
{
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Exact locale-aware integer parser and numeric suffix caller. Selected accepted boundaries; no runtime/fault/MMIO coverage.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_numeric_parse_chain61_context
