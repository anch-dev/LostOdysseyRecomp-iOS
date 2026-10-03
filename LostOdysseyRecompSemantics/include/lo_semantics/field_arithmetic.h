#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::field_arithmetic
{

// The mapped operations only touch these guest registers. The caller keeps
// every other PPCContext field intact.
struct Registers
{
    std::uint64_t r3 = 0;
    std::uint64_t r4 = 0;
    std::uint64_t r5 = 0;
    std::uint64_t r8 = 0;
    std::uint64_t r9 = 0;
    std::uint64_t r10 = 0;
    std::uint64_t r11 = 0;
};

// Returns false without side effects when address has no reviewed mapping.
[[nodiscard]] bool Apply(std::uint32_t address, Registers& registers,
                         gpu::GuestMemory& memory);

} // namespace lo::semantic::field_arithmetic

// The same operations support bounded test memory and native-width runtime memory.
#include "lo_semantics/detail/field_arithmetic_impl.h"
