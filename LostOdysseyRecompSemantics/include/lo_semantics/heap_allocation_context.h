#pragma once

#include "lo_semantics/raw_allocation_context.h"

namespace lo::semantic::gpu::heap_allocation_context
{
using Registers = raw_allocation_context::Registers;

class BoundaryServices
{
public:
    virtual ~BoundaryServices() = default;
    // The 827CC428 grow, 827CBA60 compare, and 82B7BC40 fill helpers use
    // live selected guest context. 823AD544 is implemented in this family.
    virtual void CallDirect(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
    // Kernel/RTL imports retain their live context and may mutate guest RAM.
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Full control flow of 823ACCB0 (539 PPC instructions) and 823AD544 (18),
// including all 32 GPRs, CR0/CR6, XER SO/CA, LR/CTR, and stack/RAM effects.
// Other CR fields, fault/unwind behavior, MMIO, concurrent heap mutation,
// and implementations of the explicit guest/native callees remain outside
// this selected-context interface. Unknown entries have no effects.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_allocation_context
