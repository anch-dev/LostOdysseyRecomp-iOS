#pragma once

#include "lo_semantics/metadata_utf16_slice.h"

namespace lo::semantic::gpu::metadata_utf16_search
{
using FrameRegisters = metadata_utf16_slice::FrameRegisters;

// 8229D0E8 searches UTF16 text; 82367160 constructs a clamped prefix;
// 82339D30 replaces successive matches using real recovered array helpers.
// Full input/result carriers and owned guest frame saves are observable.
// Lower helpers retain their documented ABI/service boundaries. Comparisons
// cover terminating ordinary-RAM cases, not faults, MMIO or concurrency.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& arrays, ManagerFacadeServices& manager,
    metadata_utf16_slice::VirtualServices& methods,
    std::uint64_t r3, std::uint64_t r4, std::uint64_t r5,
    std::uint64_t r6, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_utf16_search
