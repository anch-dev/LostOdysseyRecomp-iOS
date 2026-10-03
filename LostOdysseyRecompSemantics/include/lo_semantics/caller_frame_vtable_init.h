#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::caller_frame_vtable_init
{
struct Registers
{
    std::uint64_t sp = 0, lr = 0, r3 = 0, r11 = 0, r12 = 0, r31 = 0;
};

// Restore the direct object's default vtable and the two caller-frame funclets
// that invoke the recovered 828138F8 helper.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
