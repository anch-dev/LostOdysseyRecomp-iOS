#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_character_cursor
{
using Registers = crt_stream_operations::Registers;

// Read the next bounded UTF-16 token, updating the live source cursor.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_character_cursor
