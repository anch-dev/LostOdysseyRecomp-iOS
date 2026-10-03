#pragma once

#include "lo_semantics/metadata_utf16_slice.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_descriptor_names
{
struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r25 = 0, r26 = 0, r27 = 0, r28 = 0;
    std::uint64_t r29 = 0, r30 = 0, r31 = 0;
};

// 822A9668 resolves a packed name and optional numeric suffix;
// 823AC8E0 renders a bounded owner chain and appends that name. Inputs,
// result, caller SP and owned saved registers retain their full PPC widths.
// Dynamic UTF-16 vtable methods use the accepted VirtualServices boundary.
// Generic lower ABI effects beyond the existing APIs, MMIO and unbounded
// cyclic owner chains are outside this model. Unknown addresses have no effect.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_descriptor_names
