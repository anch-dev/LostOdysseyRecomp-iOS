#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu::registered_constructor_family
{

// The lazy singleton's post-construction guest function remains a named
// boundary. It is invoked even if the 376-byte allocation fails and may
// mutate the singleton global before the wrapper reloads its return value.
class RegistrationServices
{
public:
    virtual ~RegistrationServices() = default;
    virtual std::uint64_t Register(GuestAddress callback,
        std::uint64_t incoming_r3, GuestAddress caller_sp) = 0;
};

// Apply one exact 376-byte constructor wrapper or lazy singleton. caller_sp
// is guest r1 at entry. A known address sets result and returns true; unknown
// addresses return false without changing result, memory, or invoking services.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    RegistrationServices& registration_services,
    std::uint64_t incoming_r3, GuestAddress caller_sp,
    std::uint64_t& result);

} // namespace lo::semantic::gpu::registered_constructor_family
