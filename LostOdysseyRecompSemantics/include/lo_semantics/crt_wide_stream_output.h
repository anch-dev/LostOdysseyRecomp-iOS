#pragma once

#include "lo_semantics/crt_formatting_support.h"
#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_wide_stream_output
{
using Registers = crt_stream_operations::Registers;

struct Dependencies
{
    crt_stream_operations::Dependencies streams;
    crt_formatting_support::NativeServices& error_output;
};

// Three connected PPC entries. The native services and accepted lower models
// retain their documented selected-ABI boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);

// Oracle bridge for direct calls from the pinned original bodies.
[[nodiscard]] bool ApplyAcceptedCallee(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_wide_stream_output
