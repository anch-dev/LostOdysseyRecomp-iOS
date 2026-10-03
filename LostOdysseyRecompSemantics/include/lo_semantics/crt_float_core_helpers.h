#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_float_core_helpers
{
using Registers = crt_stream_operations::Registers;

struct Dependencies
{
    CrtThreadDataServices& thread;
    InvalidParameterServices& invalid;
};

// Two recovered PPC helpers. The CRT lower services remain live at their
// accepted native boundaries; unknown entries leave state and RAM unchanged.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_float_core_helpers
