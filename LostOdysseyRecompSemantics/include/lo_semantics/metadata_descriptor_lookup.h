#pragma once

#include "lo_semantics/metadata_name_lookup.h"

namespace lo::semantic::gpu::metadata_descriptor_lookup
{
using FrameRegisters = metadata_name_lookup::FrameRegisters;

// 8229D160 searches the owned/global descriptor chains by a packed name pair,
// owner, excluded flag mask and exact/ancestor type filter. 82400A30 advances
// the descriptor's suffix counter until that name pair is unused.
// Full r3-r8 and caller SP are retained; own save24/save27 frames and selected
// nonvolatile registers/LR are observable. Sentinel-name resolution composes
// the actual recovered 82296D30 algorithm. Its external service/volatile ABI
// limits carry through; faults/MMIO, access widths and concurrent tables are
// outside this ordinary-RAM boundary. Unknown addresses have no effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    metadata_name_record::Services& records, ArrayResizeServices& arrays,
    CrtThreadDataServices& threads, InvalidParameterServices& invalid,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t incoming_r8,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_descriptor_lookup
