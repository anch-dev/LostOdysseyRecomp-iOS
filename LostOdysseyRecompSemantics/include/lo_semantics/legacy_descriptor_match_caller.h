#pragma once

#include "lo_semantics/legacy_descriptor_numeric_match_routes.h"
#include "lo_semantics/legacy_descriptor_scalar_intern_callers.h"

namespace lo::semantic::gpu::legacy_descriptor_match_caller
{
struct Registers : legacy_descriptor_numeric_match_routes::Registers
{
    std::uint64_t f3_bits = 0, f4_bits = 0;
    std::uint64_t f12_bits = 0, f13_bits = 0;
};

class DiagnosticServices
{
public:
    virtual ~DiagnosticServices() = default;
    // 82F99F48 enters the mutable 82F99D98 diagnostic continuation.
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

struct Dependencies
{
    legacy_descriptor_numeric_match_routes::Services& numeric;
    legacy_descriptor_scalar_intern_callers::Dependencies scalar;
    DiagnosticServices& diagnostic;
};

// Complete 8305A868 selected control flow. The accepted selector, numeric
// extraction/match and typed scalar caller run through their actual lowers;
// only the diagnostic continuation, guest allocator and deeper imports retain
// explicit mutable boundaries. Faults, MMIO and unexposed FP state remain open.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_match_caller
