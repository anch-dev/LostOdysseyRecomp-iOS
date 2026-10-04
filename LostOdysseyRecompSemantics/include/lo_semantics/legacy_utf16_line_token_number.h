#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_utf16_line_token_number
{
using Registers = crt_stream_operations::Registers;

// 82295EE0 reads one bounded UTF-16 configuration line, preserving quotes and
// dropping comment text. 822988E0 finds a UTF-16 key and parses the decimal
// token after it. The latter composes accepted search, length and CRT integer
// parser semantics; TLS/invalid-parameter behavior stays with their services.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    CrtThreadDataServices& thread_services,
    InvalidParameterServices& invalid_services, Registers& registers);
} // namespace lo::semantic::gpu::legacy_utf16_line_token_number
