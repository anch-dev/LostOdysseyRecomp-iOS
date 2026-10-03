#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_marker_initializer_family
{

// Clear the shared marker state and bind the instance vtable. Known entries
// preserve full guest r3; unknown addresses leave memory and result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_marker_initializer_family
