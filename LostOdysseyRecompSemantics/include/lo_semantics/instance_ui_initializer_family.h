#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_ui_initializer_family
{
struct Spec
{
    GuestAddress address;
    GuestAddress first_field_offset;
    GuestAddress zero_field_offset;
    GuestAddress initial_field_word;
    GuestAddress vtable;
    GuestAddress final_field_word;
};
[[nodiscard]] const Spec* Find(GuestAddress address);

template <class Memory>
void InitializeWith(const Spec& spec, Memory& memory, GuestAddress object)
{
    if (object == 0) return;
    memory.WriteU32(object + spec.first_field_offset, spec.initial_field_word);
    memory.WriteU32(object, spec.vtable);
    memory.WriteU32(object + spec.first_field_offset, spec.final_field_word);
    memory.WriteU32(object + spec.zero_field_offset, 0u);
    memory.WriteU32(object + spec.zero_field_offset + 4u, 0u);
}

// Initialize the UI instance vtable and adjacent state in the original
// five-store order. Known entries preserve full guest r3; unknown addresses
// leave memory and result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_ui_initializer_family
