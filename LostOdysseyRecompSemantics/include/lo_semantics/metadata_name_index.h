#pragma once

#include "lo_semantics/allocation_array.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_name_index
{

struct FrameRegisters
{
    std::uint64_t lr{};
    std::uint64_t r27{};
    std::uint64_t r28{};
    std::uint64_t r29{};
    std::uint64_t r30{};
    std::uint64_t r31{};
    std::uint64_t r0{};
    std::uint64_t ctr{};
};

// 82296FE8 folds one full-r3 UTF-16 unit. Its fixed PPC switch reads a guest
// jump-table word into r0 and CTR before dispatching by the low 16-bit value.
[[nodiscard]] std::uint64_t FoldUtf16(GuestMemory& memory,
    std::uint64_t incoming_r3, FrameRegisters& frame);

// 82296F68 hashes a zero-terminated UTF-16 name using the live 256-word table
// at 0x832EE168. caller_sp is the full guest r1 at entry; the exact 112-byte
// frame save/restore is visible through memory aliases.
[[nodiscard]] std::uint64_t HashName(GuestMemory& memory,
    std::uint64_t source_register, std::uint64_t caller_sp,
    FrameRegisters& frame);

// Apply 82296FE8, 82296F68 or 823F44E8. The insertion entry uses the
// recovered ResizeArray lower contract. On a resize callback, its residual r3
// is bounded to the lower service's zero-extended 32-bit storage result;
// arbitrary full-width callback returns and generic lower volatile/frame ABI
// effects are outside that existing API. Unknown addresses have no effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& resize_services, std::uint64_t incoming_r3,
    std::uint64_t caller_sp, FrameRegisters& frame,
    std::uint64_t& result);

} // namespace lo::semantic::gpu::metadata_name_index
