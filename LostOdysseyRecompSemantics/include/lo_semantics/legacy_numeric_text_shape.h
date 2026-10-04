#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_numeric_text_shape
{
using Registers = crt_stream_operations::Registers;

// 82479188 validates the bounded UTF-16 text in a length-bearing record.
// 823F7BF8 is its complete length-minus-one lower. Guest RAM and selected
// GPR/CR6/XER state include the original stack spills and leaf scratch.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_numeric_text_shape
