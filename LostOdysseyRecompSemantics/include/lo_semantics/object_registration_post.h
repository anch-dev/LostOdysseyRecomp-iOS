#pragma once

#include "lo_semantics/registered_callback_family.h"
#include "lo_semantics/registered_constructor_family.h"
#include "lo_semantics/object_registration.h"

#include <array>

namespace lo::semantic::gpu::object_registration_post
{

struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, un = 0;
    bool operator==(const Condition&) const = default;
};

struct Registers
{
    std::array<std::uint64_t, 32> r{};
    std::uint64_t lr = 0, ctr = 0;
    std::uint8_t xer_so = 0;
    Condition cr6{};
};

class VirtualServices
{
public:
    virtual ~VirtualServices() = default;
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

struct Dependencies
{
    ManagerFacadeServices& manager;
    registered_constructor_family::RegistrationServices& constructor_registration;
    registered_callback_family::Services& callback;
    ObjectRegistrationServices& graph;
    VirtualServices& dynamic;
};

// Full selected-register control flow of 82627230. Its six direct targets
// compose existing recovered semantics; vtable+124 remains a live guest call.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

} // namespace lo::semantic::gpu::object_registration_post
