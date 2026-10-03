#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_utf16_prefix_cursor
{
using Registers = crt_stream_operations::Registers;

// 82296740 compares the next UTF-16 prefix and advances or rewinds its
// caller-owned source cursor. The three direct callees have accepted library
// implementations; invalid-parameter and native paths remain external.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_utf16_prefix_cursor
