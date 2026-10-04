#pragma once

#include "lo_semantics/crt_record_allocation_context.h"

namespace lo::semantic::gpu::crt_reallocation_context
{
using Registers = crt_async_status_transfer::Registers;

class GuestServices
{
public:
    virtual ~GuestServices() = default;
    // Real guest 823ACBD0, 823ADDC0 and 827CCF80 retain mutable context.
    // Their mapped typed models alone do not establish this complete ABI.
    virtual void CallLower(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

struct Dependencies
{
    crt_record_allocation_context::Dependencies allocation;
    GuestServices& guest;
};

// Complete 823ACAD8 control flow; mapped guest allocation/free/reallocation
// and the indirect new-handler target retain their explicit selected scope.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

[[nodiscard]] bool ApplyLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_reallocation_context
