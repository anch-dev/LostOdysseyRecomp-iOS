#pragma once

#include "lo_semantics/heap_decommit_context.h"

namespace lo::semantic::gpu::heap_decommit_range_chain
{
using Registers = heap_decommit_context::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Composes selected 827CC668 with selected 827CB498, 827CB658, and
// 827CBA60. The allocation/free kernel imports remain mutable native
// boundaries; the ABI tracks only the selected register and RAM fields.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::heap_decommit_range_chain
