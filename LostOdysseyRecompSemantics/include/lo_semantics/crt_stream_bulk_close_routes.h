#pragma once

#include "lo_semantics/crt_stream_close_pipeline.h"

namespace lo::semantic::gpu::crt_stream_bulk_close_routes
{
using Registers = crt_stream_operations::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void EnterCriticalSection(GuestMemory& memory,
        Registers& state) = 0;
    virtual void LeaveCriticalSection(GuestMemory& memory,
        Registers& state) = 0;
};

struct Dependencies
{
    crt_stream_close_pipeline::Dependencies pipeline;
    NativeServices& native;
};

// Seven complete generated bodies: two upper close/sweep callers and five
// nested lock/unlock/continuation helpers. The two catalog-external funclets
// are validation-only; the accepted close pipeline and format/lock callees
// remain connected to their actual selected PPC implementations.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_bulk_close_routes
