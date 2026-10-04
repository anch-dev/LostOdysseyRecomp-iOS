#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_stream_block_read_context
{
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Complete selected block-read/secure-copy integer control flow. DF2520
// and DF24E4 are catalog-external validation-only helpers. Existing full
// copy/fill, stream, lock and new refill/byte-read bodies retain their own
// native/guest, fault, MMIO and concurrency boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Selected lower composition for bounded original-body callers.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_block_read_context
