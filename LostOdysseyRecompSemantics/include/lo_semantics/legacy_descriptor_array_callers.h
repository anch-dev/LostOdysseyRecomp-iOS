#pragma once

#include "lo_semantics/legacy_descriptor_array_lookup.h"

namespace lo::semantic::gpu::legacy_descriptor_array_callers
{
using Registers = legacy_descriptor_array_lookup::Registers;

class DiagnosticServices
{
public:
    virtual ~DiagnosticServices() = default;
    // The 82F99F48 diagnostic shim ends in 82F99D98 without a local epilog.
    // This selected boundary may mutate all exposed registers and guest RAM.
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

struct Dependencies
{
    legacy_descriptor_array_lookup::Dependencies lookup;
    DiagnosticServices& diagnostic;
};

// Full 83058C60 pair-to-array and 83058CF0 typed-double-to-array callers.
// Their normal paths compose the accepted 83058AD8 lookup. Diagnostic
// 82F99F48's 14-instruction shim is modeled, then 82F99D98 is mutable.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_array_callers
