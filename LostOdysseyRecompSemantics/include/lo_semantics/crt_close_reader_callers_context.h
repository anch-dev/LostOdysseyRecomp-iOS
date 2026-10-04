#pragma once

#include "lo_semantics/crt_close_recursive_buffer_context.h"
#include "lo_semantics/crt_stream_block_read_context.h"
#include "lo_semantics/crt_stream_close_reopen_context.h"

namespace lo::semantic::gpu::crt_close_reader_callers_context
{
using Registers = crt_async_status_transfer::Registers;
struct Dependencies
{
    crt_close_recursive_buffer_context::GuestServices& guest;
    crt_stream_close_shared_lower::Dependencies accepted;
};

// Complete selected 82BD0A60 and 82BD0D28 caller control flow. The two
// indirect allocator targets remain explicit mutable guest callbacks.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_close_reader_callers_context
