#pragma once

#include "lo_semantics/object_curve_sample.h"

namespace lo::semantic::gpu::object_curve_box_dispatch
{
struct Registers : object_curve_sample::Registers
{
    std::uint64_t f2_bits = 0, f3_bits = 0, f4_bits = 0;
    std::uint64_t f5_bits = 0, f6_bits = 0, f7_bits = 0;
    std::uint64_t f8_bits = 0, f9_bits = 0;
    std::uint64_t f25_bits = 0, f26_bits = 0, f27_bits = 0;
    std::uint64_t f28_bits = 0, f29_bits = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
    virtual void CallVirtual(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Actual 82626410 curve-pair box dispatch. Its only direct callees are the
// accepted 82607318 selector and 822C7FB8 vector sampler. The vtable+108
// target remains a mutable guest boundary over this selected register state.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_curve_box_dispatch
