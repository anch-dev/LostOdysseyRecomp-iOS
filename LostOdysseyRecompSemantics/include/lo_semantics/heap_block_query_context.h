#pragma once

#include "lo_semantics/heap_grow_context.h"

namespace lo::semantic::gpu::heap_block_query_context
{
using Registers = heap_grow_context::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Complete selected-context 827CC218 allocation-size query. Process-type
// and bug-check imports remain explicit mutable boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::heap_block_query_context
