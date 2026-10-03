#pragma once

#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu::manager_index_tables
{

struct FrameRegisters
{
    std::uint64_t lr{};
    std::uint64_t r28{};
    std::uint64_t r29{};
    std::uint64_t r30{};
    std::uint64_t r31{};
};

// 82326B08 and 823267A0 rebuild 16/28-byte-entry bucket tables; 82326890
// appends one 28-byte entry; 823266A0 finds/upserts by a 64-bit key. This models
// ordinary mapped guest RAM, exact own-frame saves/restores, and the existing
// manager/array lower APIs. Lower generic callback register redirection is
// outside those existing APIs. Unknown addresses produce no effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager, ArrayResizeServices& arrays,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result);

} // namespace lo::semantic::gpu::manager_index_tables
