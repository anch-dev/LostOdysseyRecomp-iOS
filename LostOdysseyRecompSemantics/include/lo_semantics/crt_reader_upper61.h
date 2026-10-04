#pragma once
#include "lo_semantics/crt_stream_refill_context.h"
namespace lo::semantic::gpu::crt_reader_upper61 {
using Registers = crt_async_status_transfer::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;
// Actual wide-reader body with exact handle/buffer leaves and actual refill.
// Errno/allocation and refill native services retain their selected lower ABI.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}
