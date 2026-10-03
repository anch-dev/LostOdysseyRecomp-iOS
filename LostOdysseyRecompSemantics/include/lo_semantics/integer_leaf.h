#pragma once

#include <cstdint>

namespace lo::semantic::integer_leaf
{

// Registers read or written by this batch of pure integer leaf functions.
// Callers retain every other register and guest memory byte unchanged.
struct Registers
{
    std::uint64_t r3 = 0;
    std::uint64_t r4 = 0;
    std::uint64_t r5 = 0;
    std::uint64_t r6 = 0;
    std::uint64_t r7 = 0;
    std::uint64_t r10 = 0;
    std::uint64_t r11 = 0;
};

// Returns false for an address outside the verified integer-leaf map.
[[nodiscard]] bool Apply(std::uint32_t address, Registers& registers) noexcept;

} // namespace lo::semantic::integer_leaf
