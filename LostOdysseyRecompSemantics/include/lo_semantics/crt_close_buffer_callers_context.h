#pragma once

#include "lo_semantics/crt_close_block_output_context.h"
#include "lo_semantics/crt_close_recursive_buffer_context.h"

namespace lo::semantic::gpu::crt_close_buffer_callers_context
{
using Registers = crt_async_status_transfer::Registers;

struct Dependencies
{
    crt_close_recursive_buffer_context::GuestServices& recursive_guest;
    crt_close_block_output_context::Dependencies output;
};

// Complete selected integer caller flow of 82BD0898, 82BD0DF8 and
// 82BD1200. Their direct recursive buffer, block output, shared close and
// bulk close callees use the accepted selected implementations. The lower
// guest/native boundaries and unselected effects retain their own limits.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_close_buffer_callers_context
