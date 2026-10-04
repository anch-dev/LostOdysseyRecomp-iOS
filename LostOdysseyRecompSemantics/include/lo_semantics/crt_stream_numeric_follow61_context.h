#pragma once
#include "lo_semantics/crt_temp_path_chain61_context.h"
namespace lo::semantic::gpu::crt_stream_numeric_follow61_context
{
using Registers=crt_temp_path_chain61_context::Registers;
using Dependencies=crt_temp_path_chain61_context::Dependencies;
// Exact public temporary-stream wrapper and parent-frame cleanup entry.
// Accepted heap/open/close native selected ABI, fault/MMIO/runtime remain bounded.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies dependencies,Registers& state);
} // namespace lo::semantic::gpu::crt_stream_numeric_follow61_context
