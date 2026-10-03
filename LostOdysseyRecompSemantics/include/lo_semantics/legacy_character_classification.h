#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::legacy_character_classification
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    bool operator==(const Condition&) const = default;
};
struct Registers
{
    std::uint64_t sp = 0, lr = 0, r3 = 0, r11 = 0, r12 = 0, r31 = 0;
    std::uint8_t xer_so = 0;
    Condition cr6{};
};

// The title's legacy character ranges use the low 16 bits. Preserve their
// exact range rules rather than substituting a host locale classification.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
