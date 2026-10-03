#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/instance_component_initializer_family.h"

namespace lo::semantic::gpu::instance_fp_extension_family
{

struct FpEffects
{
    double f0{};
    double f13{};
};

// Seven complete null-guarded initializers. The callback occurs at the first
// lfs on the non-null path; FPRs reflect loaded-single values without FP
// arithmetic. The full incoming r3 is returned. Unknown addresses change
// nothing. Signaling-NaN FPR behavior can differ from generated host C++;
// the original 64-bit zero store's fault/MMIO width is outside GuestMemory.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, FpEffects& effects, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_fp_extension_family
