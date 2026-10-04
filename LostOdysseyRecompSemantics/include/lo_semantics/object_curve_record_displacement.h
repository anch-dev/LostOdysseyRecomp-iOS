#pragma once

#include "lo_semantics/object_curve_box_dispatch.h"

namespace lo::semantic::gpu::object_curve_record_displacement
{
struct Registers : object_curve_box_dispatch::Registers
{
    std::uint64_t f14_bits = 0, f15_bits = 0, f16_bits = 0;
    std::uint64_t f17_bits = 0, f18_bits = 0, f19_bits = 0;
    std::uint64_t f20_bits = 0, f21_bits = 0, f22_bits = 0;
    std::uint64_t f23_bits = 0, f24_bits = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
    virtual void CallVirtual(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// 826276B8 applies curve displacement to indexed records. Its direct
// 8229F208 normalizer is also restored here; the vtable+268 target remains
// a mutable selected-register boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_curve_record_displacement
