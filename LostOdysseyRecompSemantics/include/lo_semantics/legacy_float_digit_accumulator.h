#pragma once
#include "lo_semantics/guest_memory.h"
#include <array>
namespace lo::semantic::gpu::legacy_float_digit_accumulator
{
struct Condition
{
    std::uint8_t lt=0, gt=0, eq=0, so=0;
    bool operator==(const Condition&) const = default;
};
struct Registers
{
    std::array<std::uint64_t,32> r{};
    std::uint64_t lr=0, ctr=0;
    std::array<Condition,8> cr{};
    std::uint8_t xer_so=0, xer_ca=0;
};
// Accumulate numeric digit bytes into the normalized 12-byte extended record.
// The accepted 12-byte memory-copy lower is reused with its live control state.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
