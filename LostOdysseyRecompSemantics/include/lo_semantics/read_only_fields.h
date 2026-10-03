#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::read_only_fields
{

// The reviewed read-only functions only modify these guest registers.
struct Registers
{
    std::uint64_t r3 = 0;
    std::uint64_t r4 = 0;
    std::uint64_t r5 = 0;
    std::uint64_t r8 = 0;
    std::uint64_t r9 = 0;
    std::uint64_t r10 = 0;
    std::uint64_t r11 = 0;
    std::uint64_t r13 = 0;
    std::uint64_t r18 = 0;
};

// Returns false without changing registers or memory for an unmapped address.
[[nodiscard]] bool Apply(std::uint32_t address, Registers& registers,
                         gpu::GuestMemory& memory);

} // namespace lo::semantic::read_only_fields
