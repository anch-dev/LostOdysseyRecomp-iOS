#pragma once

#include "lo_semantics/heap_grow_context.h"

namespace lo::semantic::gpu::heap_destroy_context
{
using Registers = heap_grow_context::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Complete selected-context 827CBA08 page release and 827CBB98 heap
// destruction. Kernel imports remain mutable boundaries; only selected
// GPR/CR0/CR6/XER/LR/CTR/RAM state is represented.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::heap_destroy_context
