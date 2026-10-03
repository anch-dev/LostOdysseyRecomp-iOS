#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_marker_initializer_family
{
struct Spec { GuestAddress address; GuestAddress vtable; };
[[nodiscard]] const Spec* Find(GuestAddress address);

template <class Memory>
void InitializeWith(const Spec& spec, Memory& memory, GuestAddress object)
{
    if (object == 0) return;
    memory.WriteU32(object + 560u, 0u);
    memory.WriteU32(object + 564u, 0u);
    memory.WriteU8(object + 568u, 0u);
    memory.WriteU32(object, spec.vtable);
}

// Clear the shared marker state and bind the instance vtable. Known entries
// preserve full guest r3; unknown addresses leave memory and result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_marker_initializer_family
