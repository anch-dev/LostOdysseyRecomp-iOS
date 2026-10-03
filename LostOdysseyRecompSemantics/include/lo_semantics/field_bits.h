#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::field_bits
{

// Only these guest registers are read or written by the mapped field/bit leaves.
// The caller retains every other PPCContext field.
struct Registers
{
    std::uint64_t r3 = 0;
    std::uint64_t r4 = 0;
    std::uint64_t r11 = 0;
};

// Applies a reviewed fixed-field bit operation. Returns false without changing
// registers or memory when the address is outside this family map.
[[nodiscard]] bool Apply(std::uint32_t address, Registers& registers,
                         gpu::GuestMemory& memory);

} // namespace lo::semantic::field_bits

// The same operations support bounded test memory and native-width runtime memory.
#include "lo_semantics/detail/field_bits_impl.h"
