#pragma once

#include "lo_semantics/crt_async_status_transfer.h"
#include "lo_semantics/crt_stream_operations.h"
#include "lo_semantics/heap_allocation_context.h"
#include "lo_semantics/heap_free_context.h"

namespace lo::semantic::gpu::crt_stream_position_routes
{
using Registers = crt_async_status_transfer::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void NtQueryInformationFile(GuestMemory&, Registers&) = 0;
    virtual void NtSetInformationFile(GuestMemory&, Registers&) = 0;
};

struct Dependencies
{
    crt_stream_operations::Dependencies stream;
    heap_allocation_context::BoundaryServices& heap_allocate;
    heap_free_context::BoundaryServices& heap_free;
    NativeServices& native;
};

// Complete 82DF43F0, 82DF6948 and 82DF6B68 stream position routines,
// including the 82DF6AE0 flag and 82BE44A8 native file-position callees.
// Accepted heap bodies retain their documented mutable guest/import boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_position_routes
