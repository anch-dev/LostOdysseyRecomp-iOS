#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_stream_flush_context
{
using Registers = crt_stream_operations::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // The import receives the live selected PPC state and its 96-byte frame.
    virtual void NtFlushBuffersFile(GuestMemory& memory,
        Registers& registers) = 0;
};

// Complete selected integer control flow of 82B81F78, 82BE4898 and
// 82B820C4. Accepted CRT callees retain their established selected ABI;
// native imports, unselected CR fields, faults, MMIO and runtime remain open.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    crt_stream_operations::Dependencies accepted, NativeServices& native,
    Registers& registers);
} // namespace lo::semantic::gpu::crt_stream_flush_context
