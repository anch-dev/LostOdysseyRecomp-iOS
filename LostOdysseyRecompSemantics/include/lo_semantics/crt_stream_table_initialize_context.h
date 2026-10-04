#pragma once

#include "lo_semantics/crt_record_allocation_context.h"

namespace lo::semantic::gpu::crt_stream_table_initialize_context
{
using Registers = crt_record_allocation_context::Registers;
using Dependencies = crt_record_allocation_context::Dependencies;

// Full selected integer context and ordinary guest RAM effects of 82B81520.
// Its record allocation call uses the separately validated full selected
// context. Heap/native/handler internals and runtime effects retain that
// lower family's documented boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_table_initialize_context
