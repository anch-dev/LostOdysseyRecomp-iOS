#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::pending_record_cleanup
{
using Registers = crt_stream_operations::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    // The translated PPC body elides lwsync. This boundary retains its
    // placement without claiming host concurrency equivalence.
    virtual void LightweightSync() = 0;
};

// Clear a pending record and its counter, or enter through its caller-frame funclets.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& registers);
} // namespace lo::semantic::gpu::pending_record_cleanup
