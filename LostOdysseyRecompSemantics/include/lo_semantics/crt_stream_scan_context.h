#pragma once

#include "lo_semantics/crt_stream_buffer_growth_callers.h"
#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_stream_scan_context
{
using Registers = crt_async_status_transfer::Registers;

class GuestServices
{
public:
    virtual ~GuestServices() = default;
    // The object +28 guest target is selected at runtime. Its full live
    // register state and ordinary guest RAM must be returned to the caller.
    virtual void CallIndirect(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

struct Dependencies
{
    crt_stream_close_shared_lower::Dependencies accepted;
    crt_stream_buffer_growth_callers::Dependencies growth;
    GuestServices& guest;
};

// Complete selected 82DF4AF8 scanning body. Four character-table probes and
// two formatting leaves execute their actual PPC instruction bodies here;
// their existing mapping entries receive no new credit. Accepted lower
// contracts, the variable guest call, faults/MMIO and VMX remain bounded.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_scan_context
