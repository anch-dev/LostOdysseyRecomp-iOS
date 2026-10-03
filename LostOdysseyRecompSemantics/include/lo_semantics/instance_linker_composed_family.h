#pragma once

#include "lo_semantics/allocation_array.h"

namespace lo::semantic::gpu::instance_linker_composed_family
{

// Live nonvolatile registers at the requested entry. A nested Linker call
// restores these from guest stack words, which may have been overwritten by
// object writes. Volatile registers and generic lower-helper ABI are outside
// this bounded memory API.
struct FrameRegisters
{
    std::uint64_t lr{};
    std::uint64_t r30{};
    std::uint64_t r31{};
};

// One archive-field initializer and two frame wrappers. caller_sp is the
// incoming guest r1. Unknown addresses leave all state unchanged.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, FrameRegisters& frame, std::uint64_t& result);

} // namespace lo::semantic::gpu::instance_linker_composed_family
