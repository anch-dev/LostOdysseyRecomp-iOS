#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu
{

// 827CE240: allocate a 376-byte registered object directly through the manager
// and initialize its size-76/category-1 descriptor. caller_sp is r1 at entry.
// The allocation's low word is saved at frame+112 even on failure; successful
// allocation and initialization retain the full r3 value.
[[nodiscard]] std::uint64_t ConstructInlineManagedRegisteredObject(
    GuestMemory& memory, ManagerFacadeServices& services,
    std::uint64_t owner_register, GuestAddress caller_sp);

} // namespace lo::semantic::gpu
