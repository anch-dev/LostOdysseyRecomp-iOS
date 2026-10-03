#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_vtable_family
{
struct Spec
{
    GuestAddress address;
    GuestAddress vtable;
    GuestAddress field_offset;
    GuestAddress initial_field_word;
    GuestAddress final_field_word;
};

[[nodiscard]] const Spec* Find(GuestAddress address);

template <class Memory>
void InitializeWith(const Spec& spec, Memory& memory, GuestAddress object)
{
    if (object == 0) return;
    if (spec.field_offset != 0)
        memory.WriteU32(object + spec.field_offset, spec.initial_field_word);
    memory.WriteU32(object, spec.vtable);
    if (spec.field_offset != 0)
        memory.WriteU32(object + spec.field_offset, spec.final_field_word);
}

// Apply an exact null-guarded instance initializer. Some entries write only
// the vtable at +0; others write an additional field before and after the
// vtable, in that original order. The guest r3 register is returned in full,
// including its upper 32 bits. An unknown
// function address leaves memory and result unchanged and returns false.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_vtable_family
