#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::float_triplet_transfer
{
struct Registers
{
    std::uint64_t r3 = 0, r4 = 0, r10 = 0, r11 = 0, lr = 0;
    std::uint64_t f0_bits = 0, f13_bits = 0;
    std::uint32_t cached_fp_control = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// Move three loaded single-precision values through f0 in instruction order.
// The FPSCR cache and host flush mode are modeled for the Windows x64 oracle.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::float_triplet_transfer
