#pragma once

#include "lo_semantics/guest_memory.h"

#include <array>

namespace lo::semantic::gpu::legacy_float_binary_conversion
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, so = 0;
    bool operator==(const Condition&) const = default;
};

struct Xer
{
    std::uint8_t so = 0, ov = 0, ca = 0;
    bool operator==(const Xer&) const = default;
};

struct Registers
{
    std::array<std::uint64_t, 32> r{};
    std::uint64_t ctr = 0, lr = 0;
    Xer xer{};
    Condition cr0{}, cr6{};
};

// 822981C8 converts the 12-byte legacy floating record at r3 to a 32- or
// 64-bit binary result at r4, using the six-word format table at 83215C50.
// All integer register, condition, XER and ordered ordinary-RAM effects of
// this no-call PPC body are included. It does not touch FPR or FPSCR state.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Registers& registers);
} // namespace lo::semantic::gpu::legacy_float_binary_conversion
