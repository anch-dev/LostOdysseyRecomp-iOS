#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_property_initializer_family
{
struct Spec { GuestAddress address; GuestAddress vtable; };
[[nodiscard]] const Spec* Find(GuestAddress address);

template <class Memory>
void InitializeWith(const Spec& spec, Memory& memory, GuestAddress object)
{
    if (object == 0) return;
    memory.WriteU32(object + 68u, 1u);
    memory.WriteU32(object, spec.vtable);
    memory.WriteU32(object + 60u, 0u);
    memory.WriteU32(object + 96u, 0u);
    memory.WriteU32(object + 116u, 0u);
}

// Initialize the shared Property instance layout after a low-word null check.
// A known entry preserves the complete guest r3; an unknown address leaves
// memory and result untouched and returns false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_property_initializer_family
