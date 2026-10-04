#pragma once

#include "lo_semantics/heap_allocation_context.h"
#include "lo_semantics/legacy_config_format_dispatch.h"

namespace lo::semantic::gpu::legacy_config_format_full_heap_chain
{
using Registers = legacy_config_format_dispatch::Registers;

struct Dependencies
{
    legacy_config_format_dispatch::Dependencies caller;
    heap_allocation_context::BoundaryServices& heap;
};

// Composes the accepted 824790A8 caller and formatter/free lower models with
// selected-context 823ACAD8 -> 823ACBD0 -> 823ACCB0 -> 823AD544. The CRT
// reallocate branch for a non-null old buffer remains outside this adapter.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_config_format_full_heap_chain
