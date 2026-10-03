#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::single_write_fields
{

// Registers that the recovered single-store operations can modify. Addresses
// are 32-bit guest offsets, while PPC arithmetic still uses full 64-bit GPRs.
struct Registers
{
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0;
    std::uint64_t r9 = 0, r10 = 0, r11 = 0;
};

// Returns false without changing registers or memory for an unknown address.
[[nodiscard]] bool Apply(std::uint32_t address, Registers& registers,
                         gpu::GuestMemory& memory);

} // namespace lo::semantic::single_write_fields
