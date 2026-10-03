#pragma once

#include "lo_semantics/crt_float_core_helpers.h"
#include "lo_semantics/crt_float_environment.h"

namespace lo::semantic::gpu::crt_float_text_helpers
{
using Registers = crt_stream_operations::Registers;

struct Dependencies
{
    crt_float_core_helpers::Dependencies helpers;
    crt_float_environment::NativeServices& environment;
};

// Decimal rounding and scientific/fixed text layout. Native callbacks and
// accepted lower helpers retain their documented selected-ABI boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_float_text_helpers
