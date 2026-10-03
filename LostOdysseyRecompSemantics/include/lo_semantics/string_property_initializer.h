#pragma once

#include "lo_semantics/manager_facade.h"
#include "lo_semantics/registered_metadata_string.h"

#include <cstdint>

namespace lo::semantic::gpu::string_property_initializer
{

// Nonvolatile state and LR touched by 82496948's savegprlr_28 frame.
// 822A06C0 uses only r30/r31 and LR. The caller retains the guest r1.
struct FrameRegisters
{
    std::uint64_t lr;
    std::uint64_t r28;
    std::uint64_t r29;
    std::uint64_t r30;
    std::uint64_t r31;
};

// 822A06C0 copies a three-word string header and its current contents.
// 82496948 composes four such copies, an optional temporary UTF-16 string,
// and its cleanup. The resize and release services are separate existing
// guest callback boundaries. Unknown addresses return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result);

} // namespace lo::semantic::gpu::string_property_initializer
