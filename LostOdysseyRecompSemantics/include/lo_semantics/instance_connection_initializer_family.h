#pragma once

#include "lo_semantics/allocation_array.h"
#include "lo_semantics/instance_component_initializer_family.h"
#include "lo_semantics/manager_facade.h"

#include <cstdint>

namespace lo::semantic::gpu::instance_connection_initializer_family
{

// Guest nonvolatiles and LR at entry. The verified __savegprlr_25 and
// __savegprlr_29 helpers save these in guest memory before the frame; their
// restore helpers reload live words, which may alias object writes.
struct FrameRegisters
{
    std::uint64_t lr{};
    std::uint64_t r25{};
    std::uint64_t r26{};
    std::uint64_t r27{};
    std::uint64_t r28{};
    std::uint64_t r29{};
    std::uint64_t r30{};
    std::uint64_t r31{};
};

struct FpEffects
{
    double f0{};
    double f13{};
};

// Apply 82752768 (BitWriter), 8267A1F0 (NetConnection), or one of their
// three connection wrappers. caller_sp is the full guest entry r1. Unknown
// addresses leave all state untouched. Generic volatile/CR state, callback
// internals, signaling-NaN host casts, faults and MMIO are outside this
// bounded ordinary-memory semantic API.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, FpEffects& effects,
    std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_connection_initializer_family
