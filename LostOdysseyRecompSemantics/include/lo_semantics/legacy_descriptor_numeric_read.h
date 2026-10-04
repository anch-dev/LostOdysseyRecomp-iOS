#pragma once

#include "lo_semantics/legacy_fp_flagged_routes.h"

namespace lo::semantic::gpu::legacy_descriptor_numeric_read
{
using Registers = legacy_fp_flagged_routes::Registers;
class Services : public legacy_fp_classification::HostFpServices
{
public:
    // 82F99F48 diagnostic implementation retains this mutable selected ABI.
    virtual void Diagnostic(GuestMemory& memory, Registers& state) = 0;
};

// Complete typed scalar getter and output-record conversion caller, composing
// the accepted flag/clamp/sign chain. Faults/MMIO and diagnostic internals
// remain outside this selected register and ordinary-RAM interface.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Services& services, Registers& state);
} // namespace lo::semantic::gpu::legacy_descriptor_numeric_read
