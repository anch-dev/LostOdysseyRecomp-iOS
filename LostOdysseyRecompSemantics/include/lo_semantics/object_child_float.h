#pragma once

#include "lo_semantics/guest_memory.h"

#include <array>

namespace lo::semantic::gpu::object_child_float
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, un = 0;
    bool operator==(const Condition&) const = default;
};
struct Registers
{
    std::array<std::uint64_t, 32> r{};
    std::uint64_t lr = 0, ctr = 0;
    std::uint64_t f0_bits = 0, f1_bits = 0, f13_bits = 0;
    std::uint64_t f30_bits = 0, f31_bits = 0;
    std::uint32_t cached_fp_control = 0;
    std::uint8_t xer_so = 0, xer_ca = 0;
    Condition cr6{};
};
class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
    virtual void CallGuest(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Full selected-register control flow of 822C5E58 and its direct child
// 822C5F28. The child's 82384C08 and virtual guest call remain boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_child_float
