#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu::object_field_routes
{
struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, un = 0;
    bool operator==(const Condition&) const = default;
};

struct Registers
{
    std::uint64_t sp = 0, r3 = 0, r4 = 0, r5 = 0;
    std::uint64_t r10 = 0, r11 = 0, r12 = 0, r31 = 0, ctr = 0, lr = 0;
    std::uint64_t f0_bits = 0, f1_bits = 0, f13_bits = 0;
    std::uint32_t cached_fp_control = 0;
    std::uint8_t xer_so = 0;
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

// Selected object-field readers and routes from ppc_recomp.2.cpp.
// Guest calls remain an explicit boundary; ordinary RAM and x64 FP cache
// behavior follow the pinned PPC bodies.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_field_routes
