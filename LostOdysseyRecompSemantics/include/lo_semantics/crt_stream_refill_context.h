#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_stream_refill_context
{
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Complete 82B85A80 and its catalog-external 82B85C08 cleanup body. The
// accepted read/lock/unlock leaves retain their selected guest/native ABI.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Original-body oracle bridge to separately accepted direct leaves. It adds
// no mapping credit to those entries.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_refill_context
