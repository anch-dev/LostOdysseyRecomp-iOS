#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::crt_float_sign
{
struct Comparison
{
    std::uint8_t less = 0, greater = 0, equal = 0, unordered = 0;
    bool operator==(const Comparison&) const = default;
};

struct Registers
{
    std::uint64_t r3 = 0, r11 = 0;
    std::uint64_t f0_bits = 0, f13_bits = 0;
    std::uint32_t cached_fp_control = 0;
    Comparison cr6{};
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// 82B7EFB0: compare a guest binary64 with the live reference at 82000FE8.
// The generated x64 FPSCR cache disables FTZ/DAZ before the first load.
// Equal and unordered inputs return one; inputs below the reference return zero.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& registers);
} // namespace lo::semantic::gpu::crt_float_sign
