#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// 82B7BC40: fill with the low byte of value and return the original destination.
// Keep the original aligned word-store order; no concurrent/MMIO contract is
// inferred from tests of ordinary mapped memory.
GuestAddress FillGuestMemory(GuestMemory& memory, GuestAddress destination,
    std::uint32_t value, std::uint32_t bytes);

} // namespace lo::semantic::gpu
