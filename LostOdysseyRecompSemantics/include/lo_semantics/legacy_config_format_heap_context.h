#pragma once

#include "lo_semantics/crt_allocation.h"
#include "lo_semantics/raw_allocation_context.h"

namespace lo::semantic::gpu::legacy_config_format_heap_context
{
// The selected 823ACCB0 path for an exact-size large-list free block, with
// its save22 frame and the actual 823AD544 cleanup frame. Other heap paths
// remain with the accepted allocator's documented ABI boundary. Unsupported
// inputs return false before modifying guest state or RAM.
[[nodiscard]] bool Supports(GuestMemory& memory, GuestAddress heap,
    std::uint32_t flags, std::uint32_t bytes);
[[nodiscard]] bool Apply(GuestMemory& memory, CrtAllocationServices& services,
    raw_allocation_context::Registers& state);
} // namespace lo::semantic::gpu::legacy_config_format_heap_context
