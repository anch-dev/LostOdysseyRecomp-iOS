#pragma once

#include "lo_semantics/instance_component_initializer_family.h"
#include "lo_semantics/owned_state_initializer.h"

namespace lo::semantic::gpu::instance_owned_state_family
{

using FrameRegisters = owned_state_initializer::FrameRegisters;

// Two direct initializers, two null-guarded tails, one flag-conditional tail
// and three framed wrappers. f0_bits records the one lfs when 82693E38 runs;
// it otherwise remains unchanged. The first lfs disables flush mode through
// the existing component service. Known optimized-C++ signaling-NaN baseline
// differences are not covered. Split 64-bit guest accesses use ordinary RAM.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    owned_state_initializer::StateServices& state_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& f0_bits, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_owned_state_family
