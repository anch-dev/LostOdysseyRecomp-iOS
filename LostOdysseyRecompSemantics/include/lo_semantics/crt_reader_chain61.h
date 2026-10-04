#pragma once
#include "lo_semantics/crt_stream_close_shared_lower.h"
namespace lo::semantic::gpu::crt_reader_chain61 {
using Registers=crt_async_status_transfer::Registers;
using Dependencies=crt_stream_close_shared_lower::Dependencies;
// Exact wide pushback including its integer conversion/tail/fill leaves.
// Existing errno/allocation services retain their selected ABI; faults,
// imports, concurrency and gameplay remain separately bounded.
[[nodiscard]] bool Apply(GuestAddress,GuestMemory&,Dependencies,Registers&);
}
