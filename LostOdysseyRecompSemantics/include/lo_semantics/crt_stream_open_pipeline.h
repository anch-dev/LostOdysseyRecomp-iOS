#pragma once

#include "lo_semantics/crt_float_environment.h"
#include "lo_semantics/crt_stream_close_error.h"
#include "lo_semantics/crt_stream_open_routes_context.h"
#include "lo_semantics/crt_stream_position_routes.h"
#include "lo_semantics/crt_stream_read_routes.h"

namespace lo::semantic::gpu::crt_stream_open_pipeline
{
using Registers = crt_async_status_transfer::Registers;

struct Dependencies
{
    crt_stream_operations::Dependencies stream;
    crt_stream_open_routes_context::GuestServices& open;
    crt_stream_position_routes::Dependencies position;
    crt_stream_read_routes::Dependencies read;
    crt_stream_close_error::GuestServices& close;
    crt_float_environment::NativeServices& failure;
};

// Complete 82DF6240 control flow with real open/position/read/close bodies.
// Each lower body keeps its documented guest and native selected ABI boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Shared lower composition for the independent original-body oracle.
[[nodiscard]] bool ApplyLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_open_pipeline
