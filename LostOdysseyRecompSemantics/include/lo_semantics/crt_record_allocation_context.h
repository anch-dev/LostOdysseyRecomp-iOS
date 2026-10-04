#pragma once

#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/heap_allocation_context.h"

namespace lo::semantic::gpu::crt_record_allocation_context
{
using Registers = crt_async_status_transfer::Registers;

class HandlerServices
{
public:
    virtual ~HandlerServices() = default;
    // 82B7FE68's function-table target is a mutable guest call.
    virtual void CallNewHandler(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

struct Dependencies
{
    crt_stream_operations::Dependencies stream;
    heap_allocation_context::BoundaryServices& heap;
    HandlerServices& handler;
};

// Complete selected integer control flow of already mapped 82B81778,
// 82B816A0 and 82B7FE68. Accepted direct CRT and heap callees retain
// their separately validated selected interfaces; the new-handler target,
// native internals, faults, MMIO, concurrency and runtime remain boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Adapter for accepted direct callees used by the original-body oracle.
// These addresses receive no additional mapping credit here.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_record_allocation_context
