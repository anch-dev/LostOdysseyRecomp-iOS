#pragma once

#include "lo_semantics/crt_reallocation_context.h"

namespace lo::semantic::gpu::crt_stream_pushback_context
{
using Registers = crt_async_status_transfer::Registers;

struct Dependencies
{
    crt_reallocation_context::Dependencies reallocation;
};

// Complete 82B87CF8 integer control flow, including the selected full-context
// forms of accepted 82B81648 and 82B85C40. The latter's 823ACBD0 call is a
// mutable guest allocator boundary; imports, MMIO, faults and concurrency
// retain their separately declared limits.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_pushback_context
