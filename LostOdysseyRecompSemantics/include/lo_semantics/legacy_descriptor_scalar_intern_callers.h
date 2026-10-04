#pragma once

#include "lo_semantics/legacy_descriptor_array_lookup.h"

namespace lo::semantic::gpu::legacy_descriptor_scalar_intern_callers
{
struct Registers : legacy_descriptor_array_lookup::Registers
{
    std::uint64_t f1_bits=0, f2_bits=0, f3_bits=0, f4_bits=0;
};
class DiagnosticServices
{
public:
    virtual ~DiagnosticServices()=default;
    virtual void Call(GuestMemory& memory, Registers& registers)=0;
};
struct Dependencies
{
    legacy_descriptor_array_lookup::Dependencies lookup;
    DiagnosticServices& diagnostic;
};
// Complete scalar-word, output-record, vector and typed-number callers,
// composing accepted array lookup with selected mutable lower boundaries.
// Diagnostic implementation, unexposed context, other layouts, exceptional
// conversions, faults, MMIO, concurrent mutation and runtime remain open.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
}
