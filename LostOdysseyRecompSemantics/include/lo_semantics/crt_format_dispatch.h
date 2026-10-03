#pragma once

#include "lo_semantics/crt_stream_locks.h"
#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_format_dispatch
{
using Registers = crt_stream_operations::Registers;
using Dependencies = crt_stream_locks::Dependencies;

// Floating-point formatter address slots, ASCII case normalization, two
// decimal-text transforms with their legacy tail entries, and the fatal-error
// tail thunk. The conversion
// targets stored in the slots are separate PPC functions.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_format_dispatch
