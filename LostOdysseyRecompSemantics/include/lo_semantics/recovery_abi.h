#pragma once

#include "lo_semantics/guest_memory.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::recovery_abi
{

// Guest addresses use the low 32 bits of a 64-bit PPC register.
[[nodiscard]] constexpr GuestAddress Address(std::uint64_t value)
{
    return static_cast<GuestAddress>(value);
}

// These are ordered, big-endian pairs of U32 accesses. They do not establish
// the original instruction's access width, atomicity, or fault behavior.
[[nodiscard]] inline std::uint64_t ReadU64(const GuestMemory& memory,
    GuestAddress address)
{
    const auto high = memory.ReadU32(address);
    const auto low = memory.ReadU32(address + 4u);
    return (std::uint64_t{high} << 32) | low;
}

inline void WriteU64(GuestMemory& memory, GuestAddress address,
    std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

// PPC word rotate in a 64-bit register: repeat the low word before rotating.
// Keep the entire 64-bit mask; truncating it to a word loses high-bit results.
[[nodiscard]] constexpr std::uint64_t WordRotateMask(std::uint64_t value,
    int rotation, std::uint64_t mask)
{
    const auto word = static_cast<std::uint32_t>(value);
    const auto repeated = (std::uint64_t{word} << 32) | word;
    return std::rotl(repeated, rotation) & mask;
}

} // namespace lo::semantic::gpu::recovery_abi
