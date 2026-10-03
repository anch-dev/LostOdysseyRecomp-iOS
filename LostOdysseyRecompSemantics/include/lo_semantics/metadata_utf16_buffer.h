#pragma once

#include "lo_semantics/manager_facade.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_utf16_buffer
{

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r28 = 0;
    std::uint64_t r29 = 0;
    std::uint64_t r30 = 0;
    std::uint64_t r31 = 0;
};

// 8232D378 appends a UTF-16 string, replacing an existing terminator.
// 8232D418 composes two recovered header copies around that append and then
// releases its temporary array. r3-r5 and caller_sp retain their full widths;
// an unknown address leaves every argument, service and guest byte untouched.
// Resize/release services retain their existing guest32 and generic lower ABI
// boundaries; own guest-frame saves and ordered memory operations are modeled.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services,
    ManagerFacadeServices& manager_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_utf16_buffer
