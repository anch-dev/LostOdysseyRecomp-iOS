#pragma once

#include "lo_semantics/heap_grow_context.h"

namespace lo::semantic::gpu::heap_grow_full_chain
{
using Registers = heap_grow_context::Registers;

class BoundaryServices
{
public:
    virtual ~BoundaryServices() = default;
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
    virtual void CallIndirect(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Composes selected 827CC428 with selected 827CB778, 827CC2C0,
// 823AE108, 827CBA60, 827CB498 and 827CB658. Kernel VM imports and a
// configured indirect guest call remain explicit mutable boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_grow_full_chain
