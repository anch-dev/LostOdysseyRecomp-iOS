#pragma once

#include "lo_semantics/legacy_fp_classification.h"


namespace lo::semantic::gpu::legacy_fp_numeric_routes
{
struct Registers
{
    legacy_fp_classification::Registers classifier{};
    std::uint64_t f31_bits = 0;
};

// Three complete CRT binary64 numeric routes. Both callers compose the
// recovered 82B7DFC0 classifier; 83053610 also composes 82FF99E0.
// The selected PPC register, cached FP-control and ordinary-RAM effects are
// exposed. Faults and MMIO remain outside this selected state.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    legacy_fp_classification::HostFpServices& services,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_fp_numeric_routes
