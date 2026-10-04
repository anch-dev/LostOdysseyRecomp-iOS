#pragma once

#include "lo_semantics/heap_growth_lower_context.h"

namespace lo::semantic::gpu::heap_create_context
{
using Registers = heap_growth_lower_context::Registers;

class BoundaryServices : public heap_growth_lower_context::BoundaryServices
{
public:
    // 82B7C470 is an accepted typed move, but its complete selected live
    // scratch is not part of this interface yet.
    virtual void CallGuestMove(GuestMemory& memory, Registers& state) = 0;
};

// Complete selected 827CC9D0 heap creation, composed with selected segment,
// range, insert and accepted memory fill. Kernel services and 82B7C470's
// selected-context ABI remain explicit mutable boundaries. Only selected
// GPR/CR0/CR6/XER/LR/CTR/RAM state is represented.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_create_context
