#pragma once

#include "lo_semantics/crt_formatter.h"

namespace lo::semantic::gpu::crt_format_stream
{
using Registers = crt_formatter::Registers;

struct Dependencies
{
    crt_formatter::Dependencies formatter;
};

// Seven connected CRT stream-formatting, locking, buffering, and cleanup
// entries. Native critical-section imports remain live service boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_format_stream
