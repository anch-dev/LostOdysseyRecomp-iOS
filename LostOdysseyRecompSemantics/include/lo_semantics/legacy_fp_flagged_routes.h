#pragma once

#include "lo_semantics/legacy_fp_numeric_routes.h"

namespace lo::semantic::gpu::legacy_fp_flagged_routes
{
struct Registers
{
    legacy_fp_numeric_routes::Registers numeric{};
    std::uint64_t f2_bits = 0;
};

// Two flag-driven binary64 routes and their complete sign helpers. The
// callers compose the recovered 83053360 clamp/classifier chain directly.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    legacy_fp_classification::HostFpServices& services,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_fp_flagged_routes
