#pragma once

#include "lo_semantics/legacy_descriptor_array_allocation.h"

namespace lo::semantic::gpu::legacy_descriptor_value_intern
{
struct Registers : legacy_descriptor_array_allocation::Registers
{
    std::uint64_t f1_bits = 0, f2_bits = 0, f3_bits = 0, f4_bits = 0;
};
using Services = legacy_descriptor_array_allocation::Services;

// Actual float-word bucket lookup and descriptor reuse/construction. The
// accepted array constructor/allocator retain their selected native boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Services& services, Registers& state);
} // namespace lo::semantic::gpu::legacy_descriptor_value_intern
