#pragma once

#include "lo_semantics/object_curve_sample.h"

namespace lo::semantic::gpu::object_curve_pair_apply
{
using Registers = object_curve_sample::Registers;
using NativeServices = object_curve_sample::NativeServices;

// Actual 822CEAB0 record update and 822C7FB8 vector sampler. Both direct
// paths enter the accepted 822C73E8 curve body; vtable+268 remains dynamic.
// Selected GPR/FPR/CR and finite normal FP behavior are validated by oracle.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_curve_pair_apply
