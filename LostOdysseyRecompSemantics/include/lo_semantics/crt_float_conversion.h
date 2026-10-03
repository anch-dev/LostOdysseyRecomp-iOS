#pragma once

#include "lo_semantics/crt_float_core_helpers.h"
#include "lo_semantics/crt_float_environment.h"

namespace lo::semantic::gpu::crt_float_conversion
{
using Registers = crt_stream_operations::Registers;

struct Dependencies
{
    crt_float_core_helpers::Dependencies helpers;
    crt_float_environment::NativeServices& environment;
};

// Convert a packed extended value into the CRT decimal record, or convert a
// binary64 value and copy that record into its caller-owned buffer. Power
// factors and text constants remain live in guest memory.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_float_conversion
