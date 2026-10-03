#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_field_initializer_family
{

// Initialize the two adjacent instance fields and vtable in original store
// order. The guest r3 register is returned in full. Unknown addresses leave
// memory and result untouched and return false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_field_initializer_family
