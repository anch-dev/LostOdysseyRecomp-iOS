#pragma once

#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu::array_release_family
{

// The reviewed array-release entrypoints differ only in element size and
// resize argument. An unknown address leaves result and guest state unchanged.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress array,
    GuestAddress caller_sp, std::uint64_t& result);

} // namespace lo::semantic::gpu::array_release_family
