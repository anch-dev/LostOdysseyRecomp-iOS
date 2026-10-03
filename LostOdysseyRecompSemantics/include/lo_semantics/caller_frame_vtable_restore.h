#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::caller_frame_vtable_restore
{
struct Registers
{
    std::uint64_t sp = 0, lr = 0, r3 = 0, r11 = 0, r12 = 0, r31 = 0;
};

// Three unwind funclets restore the default vtable of a direct frame member
// or the object referenced by that member, using the recovered field helper.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
