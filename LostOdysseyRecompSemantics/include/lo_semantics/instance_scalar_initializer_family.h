#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::instance_scalar_initializer_family
{
enum class Layout : std::uint8_t { ClearBeforeVtable, FourZerosAndEight, ThreeZeros };
struct Spec
{
    GuestAddress address;
    GuestAddress vtable;
    std::uint16_t first_field;
    Layout layout;
};
[[nodiscard]] const Spec* Find(GuestAddress address);

template <class Memory>
void InitializeWith(const Spec& spec, Memory& memory, GuestAddress object)
{
    if (object == 0) return;
    if (spec.layout == Layout::ClearBeforeVtable)
    {
        memory.WriteU32(object + spec.first_field, 0);
        memory.WriteU32(object, spec.vtable);
    }
    else
    {
        memory.WriteU32(object, spec.vtable);
        const unsigned zeros = spec.layout == Layout::FourZerosAndEight ? 4u : 3u;
        for (unsigned index = 0; index < zeros; ++index)
            memory.WriteU32(object + spec.first_field + index * 4u, 0);
        if (spec.layout == Layout::FourZerosAndEight)
            memory.WriteU32(object + spec.first_field + 16u, 8);
    }
}

// Initialize one of the reviewed null-guarded integer layouts. Store order is
// preserved, including the clear-before-vtable variant. The full incoming r3
// is returned. Unknown addresses leave memory and result untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_scalar_initializer_family
