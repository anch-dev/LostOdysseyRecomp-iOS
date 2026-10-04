#pragma once

#include "lo_semantics/crt_stream_refill_context.h"

namespace lo::semantic::gpu::crt_stream_byte_read_context
{
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Complete selected 82B81360 byte-read control flow. Refill and accepted
// stream state/error callees retain their separately documented boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_byte_read_context
