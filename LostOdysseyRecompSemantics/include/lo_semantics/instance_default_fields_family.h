#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_default_fields_family
{

// Apply the reviewed null-guarded default-field writes in their original
// order, including repeated fields and the one byte-width store. The full
// incoming r3 is returned. Unknown addresses leave memory and result alone.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_default_fields_family
