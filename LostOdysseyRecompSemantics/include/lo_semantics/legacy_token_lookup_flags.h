#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_token_lookup_flags
{
using Registers = crt_stream_operations::Registers;

// 822972A8 searches a chained table by a two-word key. 822974B0
// intersects a low-16 character's table flags with the requested mask.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_token_lookup_flags
