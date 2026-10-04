#pragma once

#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/crt_thread_error_routes.h"

namespace lo::semantic::gpu::crt_stream_open_routes_context
{
using Registers=crt_stream_operations::Registers;

class GuestServices : public crt_thread_error_routes::NativeServices
{
public:
    // 82B81778 is mapped through a typed allocator, but its complete live
    // selected ABI is not exposed. Keep this guest call explicitly mutable.
    virtual void AllocateCrtRecord(GuestMemory&,Registers&) = 0;
    virtual void EnterCriticalSection(GuestMemory&,Registers&) = 0;
    virtual void LeaveCriticalSection(GuestMemory&,Registers&) = 0;
    virtual void InitAnsiString(GuestMemory&,Registers&) = 0;
    virtual void CallOpenFile(GuestAddress target,GuestMemory&,Registers&) = 0;
};

// Complete selected integer control flow of 82B86108, 82B86420,
// 82B86630, 82B86654 and 82BE2BE0. Accepted lower adapters and native,
// guest, fault/MMIO/concurrency/runtime boundaries remain explicit.
[[nodiscard]] bool Apply(GuestAddress entry,GuestMemory& memory,
    crt_stream_operations::Dependencies accepted,GuestServices& services,
    Registers& state);

// Selected ABI adaptation for already accepted direct CRT callees used by
// the original-body oracle and this cohort; it grants no new entry credit.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& memory,
    crt_stream_operations::Dependencies accepted,GuestServices& services,
    Registers& state);
} // namespace lo::semantic::gpu::crt_stream_open_routes_context
