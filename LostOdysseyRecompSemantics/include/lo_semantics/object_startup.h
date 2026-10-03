#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/object_registration.h"

namespace lo::semantic::gpu
{

class ObjectStartupServices
{
public:
    virtual ~ObjectStartupServices() = default;

    virtual std::uint64_t CallMethod(GuestAddress method,
        std::uint64_t object_register) = 0;
    virtual std::uint64_t WaitForRetry(std::uint64_t zero_register) = 0;
};

// 823F8980: mark an eligible object as preparing, then call vtable +32.
[[nodiscard]] std::uint64_t PrepareObject(GuestMemory& memory,
    ObjectStartupServices& services, std::uint64_t object_register);

// 823F89F8: mark an eligible object as started, then call vtable +40.
[[nodiscard]] std::uint64_t CompleteObjectStartup(GuestMemory& memory,
    ObjectStartupServices& services, std::uint64_t object_register);

// 823F8A68: prepare, poll vtable +36 until it succeeds, then complete.
// The object and the three vtable slots are reloaded after callbacks.
[[nodiscard]] std::uint64_t StartObject(GuestMemory& memory,
    ObjectStartupServices& services, std::uint64_t object_register);

// 82406B00: lazily construct the primary registered object and complete its
// graph registration. The global is reloaded after the registration call.
[[nodiscard]] std::uint64_t GetPrimaryRegisteredObject(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    ObjectRegistrationServices& registration_services,
    GuestAddress caller_sp);

} // namespace lo::semantic::gpu
