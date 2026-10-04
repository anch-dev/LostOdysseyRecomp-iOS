#pragma once

#include "lo_semantics/crt_async_status_transfer.h"

namespace lo::semantic::gpu::crt_close_recursive_buffer_context
{
using Registers = crt_async_status_transfer::Registers;

class GuestServices
{
public:
    virtual ~GuestServices() = default;
    // The table's allocation function is a mutable guest target.
    virtual void CallIndirect(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Complete selected integer flow of 82BD0798, 82BD07D8 and the bounded
// self-recursion of 82BD0C18. Ordinary RAM and all 32 GPRs are retained;
// the function-table target, native internals, faults, MMIO, concurrency and
// runtime remain explicit boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    GuestServices& guest, Registers& state);
} // namespace lo::semantic::gpu::crt_close_recursive_buffer_context
