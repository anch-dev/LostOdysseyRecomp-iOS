#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::object_pointer_routes
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, un = 0;
    bool operator==(const Condition&) const = default;
};

struct Registers
{
    std::uint64_t r3 = 0, r4 = 0, r7 = 0, r8 = 0;
    std::uint64_t r9 = 0, r10 = 0, r11 = 0;
    std::uint8_t xer_so = 0;
    Condition cr6{};
};

// Exact selected-register and ordinary RAM behavior of two adjacent ppc2
// object pointer lookups. Neither original body calls another guest function.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& state);
} // namespace lo::semantic::gpu::object_pointer_routes
