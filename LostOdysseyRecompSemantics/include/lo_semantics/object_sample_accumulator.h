#pragma once

#include "lo_semantics/object_curve_sample.h"

namespace lo::semantic::gpu::object_sample_accumulator
{
using Registers = object_curve_sample::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// Actual 822C79B0 accumulator and its sole direct 82607318 selector.
// Validation covers selected GPR/FPR/CR/FP-control and finite normal FP.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);

} // namespace lo::semantic::gpu::object_sample_accumulator
