#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_property_initializer_family
{

// Initialize the shared Property instance layout after a low-word null check.
// A known entry preserves the complete guest r3; an unknown address leaves
// memory and result untouched and returns false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_property_initializer_family
