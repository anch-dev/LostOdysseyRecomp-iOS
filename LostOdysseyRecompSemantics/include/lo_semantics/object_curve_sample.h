#pragma once

#include "lo_semantics/object_child_float.h"

namespace lo::semantic::gpu::object_curve_sample
{

struct Registers : object_child_float::Registers
{
    std::uint64_t f10_bits = 0, f11_bits = 0, f12_bits = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
    virtual void CallVirtual(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Actual 822C7388 -> 822C73E8 -> 822C78D8 curve sampling chain.
// Only the guest vtable+268 call remains a mutable dynamic boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);

} // namespace lo::semantic::gpu::object_curve_sample
