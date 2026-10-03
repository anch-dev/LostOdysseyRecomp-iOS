#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu::registered_callback_family
{

// Only callees outside the recovered constructor/registration families remain
// service boundaries. Each receives the guest r1 at its own call site.
class Services
{
public:
    virtual ~Services() = default;
    virtual std::uint64_t CallExternalGetter(GuestAddress target,
        std::uint64_t incoming_r3, GuestAddress caller_sp) = 0;
    virtual std::uint64_t CallExternalConstructor(GuestAddress target,
        std::uint64_t incoming_r3, GuestAddress caller_sp) = 0;
    virtual std::uint64_t CallExternalRegistration(GuestAddress target,
        std::uint64_t incoming_r3, GuestAddress caller_sp) = 0;
    virtual std::uint64_t RegisterSecondary(std::uint64_t incoming_r3,
        GuestAddress caller_sp) = 0; // 824084F0
    virtual std::uint64_t CallReadyMethod(GuestAddress method,
        std::uint64_t receiver, GuestAddress caller_sp) = 0;
};

// Apply a 376-byte object's post-construction graph link callback. The
// constructor callback's entry r3 and r1 are supplied as incoming_r3 and
// caller_sp. Unknown addresses return false without effects.
[[nodiscard]] bool LinkRegisteredObject(GuestAddress address,
    GuestMemory& memory, ManagerFacadeServices& manager_services,
    Services& services, std::uint64_t incoming_r3,
    GuestAddress caller_sp, std::uint64_t& result);

// 82403200: link the shared metadata object. The function may recursively
// call itself if the singleton remains empty after construction.
[[nodiscard]] std::uint64_t RegisterSharedMetadataObject(GuestMemory& memory,
    ManagerFacadeServices& manager_services, Services& services,
    std::uint64_t incoming_r3, GuestAddress caller_sp);

} // namespace lo::semantic::gpu::registered_callback_family
