#pragma once

#include "lo_semantics/object_curve_sample.h"

namespace lo::semantic::gpu::object_record_property_apply
{
struct Registers : object_curve_sample::Registers
{
    std::uint64_t f7_bits = 0, f8_bits = 0, f9_bits = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// Actual 822C8430 record update and both 822C85B0/822C7B88 lookups.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);

} // namespace lo::semantic::gpu::object_record_property_apply
