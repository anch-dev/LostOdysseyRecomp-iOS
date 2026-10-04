#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::legacy_config_dispatch_gate
{
using Registers = crt_stream_operations::Registers;

class VirtualCalls
{
public:
    virtual ~VirtualCalls() = default;
    // The vtable+4 tail target receives live selected PPC state and RAM.
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

// Complete 82479058 global/bit gate and tail dispatch. Native vtable internals,
// faults, MMIO, concurrency and the full PPC context are outside this boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    VirtualCalls& virtual_calls, Registers& registers);
} // namespace lo::semantic::gpu::legacy_config_dispatch_gate
