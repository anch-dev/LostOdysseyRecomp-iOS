#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_flag_initializer_family
{

// Bind the SceneCapture instance vtable and set bit 31 of its existing
// +120 flag word. A known entry preserves full r3; an unknown address leaves
// memory and result untouched and returns false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_flag_initializer_family
