#pragma once

#include "lo_semantics/crt_format_stream.h"
#include "lo_semantics/crt_free_context.h"
#include "lo_semantics/crt_stream_close_caller.h"

namespace lo::semantic::gpu::crt_stream_close_pipeline
{
using Registers = crt_stream_operations::Registers;

struct Dependencies
{
    crt_stream_close_caller::Dependencies close;
    crt_format_stream::Dependencies format;
    crt_free_context::LowerCalls& free_lower;
};

// Complete generated 82B85CC8 stream-close pipeline and 82B87C90 buffer
// release bodies. The accepted free context retains its selected guest heap
// lower boundary; all native imports remain within accepted dependencies.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_close_pipeline
