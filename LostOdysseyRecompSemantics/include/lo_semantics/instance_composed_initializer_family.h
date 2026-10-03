#pragma once

#include "lo_semantics/instance_component_initializer_family.h"
#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu::instance_composed_initializer_family
{

// Nonvolatile registers and LR entering the requested function. They are
// restored from live guest stack words, including writes by nested callbacks.
// Volatile GPRs, CR/FPSCR details other than DisableFlushMode, and the lower
// helpers' generic ABI spills are separate boundaries. The component path
// uses LoadedSingle's ISA bit movement; the optimized PPC C++ baseline has a
// known signaling-NaN differential and is not claimed to match that input.
struct FrameRegisters
{
    std::uint64_t lr{};
    std::uint64_t r30{};
    std::uint64_t r31{};
};

// Four bounded callees, four null-guarded tails and three frame wrappers.
// caller_sp is the incoming guest r1; result retains the full PPC r3.
// Unknown addresses leave all state unchanged.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services, ArrayResizeServices& array_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, GuestAddress caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_composed_initializer_family
