#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// 827C5B30. Initializes 8192 linked entries and returns the full incoming
// r3 register. The guest head address is base+0x28000.
[[nodiscard]] std::uint64_t InitializeStorageBuckets(GuestMemory& memory,
    std::uint64_t base_register);

// 827C5D88. Builds four size groups, a 32769-entry lookup for each group,
// and then calls InitializeStorageBuckets at manager+0x20DE0. Its 144-byte
// frame contains only generic ABI saves/backchain, owned by the caller adapter.
// The full r3 register returned is manager_register+0x20DE0.
[[nodiscard]] std::uint64_t InitializePrimaryManagerStorage(GuestMemory& memory,
    std::uint64_t manager_register);

} // namespace lo::semantic::gpu
