#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::crt_exception_predicates
{
struct Registers
{
    std::uint64_t sp = 0, r3 = 0, r8 = 0, r10 = 0, r11 = 0, r12 = 0, r31 = 0;
};

// 82B80480 reads a byte in the caller's frame; 82B8223C checks the
// double-indirect exception status against STATUS_NO_MEMORY (C0000017).
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
