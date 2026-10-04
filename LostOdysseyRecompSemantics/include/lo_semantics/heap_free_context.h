#pragma once

#include "lo_semantics/raw_allocation_context.h"

namespace lo::semantic::gpu::heap_free_context
{
using Registers = raw_allocation_context::Registers;

class BoundaryServices
{
public:
    virtual ~BoundaryServices() = default;
    // Accepted guest helpers retain a mutable selected PPC context. This
    // interface does not substitute their typed business implementations.
    virtual void CallDirect(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
    // Kernel imports retain live registers and may mutate guest RAM.
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Complete selected-context flow of 823ADE28 and its 823AE0BC cleanup:
// 32 GPR, CR0/CR6, XER SO/CA, LR/CTR, and guest stack/RAM. Deep guest
// callees 823AE108, 827CBA60, and 827CC668 remain explicit mutable calls.
// Other CR fields, faults, unwind, MMIO, concurrency and kernel behavior are
// outside this interface.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    BoundaryServices& services, Registers& state);
} // namespace lo::semantic::gpu::heap_free_context
