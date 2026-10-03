#pragma once

#include "lo_semantics/string_property_initializer.h"

namespace lo::semantic::gpu::instance_property_chain_family
{

// The compositions touch LR and r28-r31 through their own and nested frames.
// Lower service callbacks, volatile GPRs and CR are bounded separately.
using FrameRegisters = string_property_initializer::FrameRegisters;

// Four callees, four low-word-null tail entries and one framed wrapper.
// Result retains the original function's full r3, including when the low word
// is null. Unknown addresses leave memory, registers and result unchanged.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, ManagerFacadeServices& manager_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_property_chain_family
