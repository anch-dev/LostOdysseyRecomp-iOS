#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// 82B7A0B0. The forward path stores the full destination register at
// stack_pointer - 8 and reloads it on return. Source, count, and memory
// addressing use their low 32 bits; the full count matters to the backward
// 82B7C470 return register.
[[nodiscard]] std::uint64_t CopyGuestMemory(GuestMemory& memory,
    std::uint64_t destination_register, GuestAddress source,
    std::uint64_t count_register, GuestAddress stack_pointer);

// 82B7C470 selects the forward tail call by signed 32-bit address comparison.
// Its backward path has no stack spill and returns the live 64-bit r3 value.
[[nodiscard]] std::uint64_t MoveGuestMemory(GuestMemory& memory,
    std::uint64_t destination_register, GuestAddress source,
    std::uint64_t count_register, GuestAddress stack_pointer);

} // namespace lo::semantic::gpu
