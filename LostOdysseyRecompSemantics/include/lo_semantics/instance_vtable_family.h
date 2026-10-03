#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_vtable_family
{

// Apply an exact null-guarded instance initializer. Some entries write only
// the vtable at +0; others write an additional field before and after the
// vtable, in that original order. The guest r3 register is returned in full,
// including its upper 32 bits. An unknown
// function address leaves memory and result unchanged and returns false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_vtable_family
