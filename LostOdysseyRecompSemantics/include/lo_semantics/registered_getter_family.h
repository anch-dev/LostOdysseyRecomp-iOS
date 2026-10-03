#pragma once

#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::registered_getter_family
{

// Fetch a registered singleton, constructing and registering it on first use.
// A registration callback may replace the global even after allocation fails.
// caller_sp is guest r1 at entry. Unknown addresses return false without effects.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    std::uint64_t incoming_r3, GuestAddress caller_sp, std::uint64_t& result);

} // namespace lo::semantic::gpu::registered_getter_family
