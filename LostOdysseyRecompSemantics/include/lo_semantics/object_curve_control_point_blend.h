#pragma once

#include "lo_semantics/object_curve_record_displacement.h"

namespace lo::semantic::gpu::object_curve_control_point_blend
{
struct Registers : object_curve_record_displacement::Registers
{ object_child_float::Condition cr0{}; };

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
};

// Actual 8262B610 record control-point transition and its direct 8262C4B0
// six-float blend. Selected full GPR/FPR, CR0/CR6, XER, CTR and stack state
// are exposed; finite normal FP and valid record spans are the oracle scope.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::object_curve_control_point_blend
