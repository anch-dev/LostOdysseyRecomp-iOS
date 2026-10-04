#pragma once
#include "lo_semantics/crt_numeric_parse_chain61_context.h"
#include "lo_semantics/raw_allocation_context.h"
namespace lo::semantic::gpu::crt_temp_path_chain61_context
{
using Registers=crt_async_status_transfer::Registers;
struct Dependencies
{
    crt_stream_close_shared_lower::Dependencies accepted;
    raw_allocation_context::PpcBoundaryServices& allocation;
};
// Complete temporary-path bodies with accepted selected heap/open/close boundaries.
// Native volatile, fault/MMIO/concurrency/runtime behavior remains bounded.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state);
} // namespace lo::semantic::gpu::crt_temp_path_chain61_context
