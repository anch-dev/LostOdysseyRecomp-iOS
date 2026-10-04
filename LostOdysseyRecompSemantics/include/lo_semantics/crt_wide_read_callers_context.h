#pragma once
#include "lo_semantics/crt_reader_upper61.h"
#include "lo_semantics/crt_stream_byte_read_context.h"
#include "lo_semantics/crt_stream_pushback_context.h"
namespace lo::semantic::gpu::crt_wide_read_callers_context {
using Registers = crt_async_status_transfer::Registers;
struct Dependencies {
    crt_stream_close_shared_lower::Dependencies accepted;
    crt_stream_pushback_context::Dependencies pushback;
};
// Complete wide-reader and locked pushback caller bodies, including their
// exact integer leaves and external unlock funclet. Accepted actual reader,
// pushback, errno/allocation and native services retain their selected ABI.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, Dependencies, Registers&);
}
