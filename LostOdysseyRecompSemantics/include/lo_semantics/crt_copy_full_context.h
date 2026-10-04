#pragma once

#include "lo_semantics/crt_async_status_transfer.h"

namespace lo::semantic::gpu::crt_copy_full_context
{
// Exact selected integer context for the already accepted 82B7A0B0 copy.
// The data-cache hints have no ordinary RAM effect. Faults, MMIO and host
// cache timing remain outside this semantic boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    crt_async_status_transfer::Registers& state);
} // namespace lo::semantic::gpu::crt_copy_full_context
