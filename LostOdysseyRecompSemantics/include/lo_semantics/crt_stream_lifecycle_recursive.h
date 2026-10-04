#pragma once

#include "lo_semantics/crt_stream_flush_context.h"
#include "lo_semantics/crt_stream_shutdown_sweep.h"

namespace lo::semantic::gpu::crt_stream_lifecycle_recursive
{
using Registers = crt_stream_operations::Registers;

struct Dependencies
{
    crt_stream_shutdown_sweep::Dependencies shutdown;
    crt_stream_flush_context::NativeServices& flush;
};

// Six complete PPC bodies in the CRT stream lifecycle group. 82B7BC30
// tail-enters 82B7B950, and 82B7B8D0 may re-enter it with mode zero.
// Accepted lower bodies retain their documented selected ABI boundaries.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,
    Dependencies dependencies,Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_lifecycle_recursive
