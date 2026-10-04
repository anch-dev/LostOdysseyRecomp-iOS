#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_close_block_output_context
{
using Registers = crt_async_status_transfer::Registers;

class ErrorOutputServices
{
public:
    virtual ~ErrorOutputServices() = default;
    // The existing 823ADD70 typed output excludes its live ABI. This guest
    // boundary keeps all selected registers mutable rather than inventing it.
    virtual void CallOutput(GuestMemory& memory, Registers& state) = 0;
};

struct Dependencies
{
    crt_stream_close_shared_lower::Dependencies accepted;
    ErrorOutputServices& output;
};

// 82DF27B0, 82DF2538 and catalog-external 82DF2884. Complete selected
// integer control flow; accepted lower/native and output ABI limits remain.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Selected accepted lower composition reused by bounded original-body tests.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_close_block_output_context
