#pragma once

#include "lo_semantics/crt_float_conversion.h"

namespace lo::semantic::gpu::crt_float_formatting
{
using Registers = crt_stream_operations::Registers;
using Dependencies = crt_float_conversion::Dependencies;

// Six PPC selectors/formatters. The selectors tail-dispatch within this
// family; binary64 values and formatting state remain guest-owned.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_float_formatting
