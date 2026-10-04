#pragma once

#include "lo_semantics/crt_reallocation_context.h"
#include "lo_semantics/heap_block_query_context.h"

namespace lo::semantic::gpu::crt_stream_resize_context
{
using Registers = crt_async_status_transfer::Registers;

struct Dependencies
{
    crt_reallocation_context::Dependencies reallocation;
    heap_block_query_context::NativeServices& heap_native;
};

// Complete selected integer context and ordinary guest RAM control flow of
// 82B82100 (allocation-size query) and 82B7D330 (resizing with zeroed tail).
// Accepted stream, raw allocation, heap query, CRT reallocation and fill
// callees retain their separately documented native/guest boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_resize_context
