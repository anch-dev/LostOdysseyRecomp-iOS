#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_field_initializer_family
{
struct Spec
{
    GuestAddress address;
    GuestAddress first_field_offset;
    GuestAddress first_initial_word;
    GuestAddress second_initial_word;
    GuestAddress vtable;
    GuestAddress first_final_word;
    GuestAddress second_final_word;
};

[[nodiscard]] const Spec* Find(GuestAddress address);

template <class Memory>
void InitializeWith(const Spec& spec, Memory& memory, GuestAddress object)
{
    if (object == 0) return;
    memory.WriteU32(object + spec.first_field_offset, spec.first_initial_word);
    memory.WriteU32(object + spec.first_field_offset + 4u, spec.second_initial_word);
    memory.WriteU32(object, spec.vtable);
    memory.WriteU32(object + spec.first_field_offset, spec.first_final_word);
    memory.WriteU32(object + spec.first_field_offset + 4u, spec.second_final_word);
}

// Initialize the two adjacent instance fields and vtable in original store
// order. The guest r3 register is returned in full. Unknown addresses leave
// memory and result untouched and return false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_field_initializer_family
