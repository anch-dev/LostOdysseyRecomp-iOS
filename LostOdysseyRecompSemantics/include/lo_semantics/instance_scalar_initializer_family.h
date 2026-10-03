#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_scalar_initializer_family
{

// Initialize one of the reviewed null-guarded integer layouts. Store order is
// preserved, including the clear-before-vtable variant. The full incoming r3
// is returned. Unknown addresses leave memory and result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_scalar_initializer_family
