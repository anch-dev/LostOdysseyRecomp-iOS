#pragma once

#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::object_float_record
{

struct Condition
{
    std::uint8_t lt = 0, gt = 0, eq = 0, un = 0;
    bool operator==(const Condition&) const = default;
};

// Selected 82384C08 state. Its accepted constructor lower is observed through
// r3, guest memory, and registration callbacks, not generic volatile GPRs.
struct Registers
{
    std::uint64_t sp = 0, lr = 0;
    std::uint64_t r3 = 0, r11 = 0, r12 = 0, r31 = 0;
    std::uint8_t xer_so = 0;
    Condition cr6{};
};

// Follow the parent object's +52/+60 list against lazy singleton 8242D038.
// That accepted lower's 82627230 registration call remains a guest service.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);

} // namespace lo::semantic::gpu::object_float_record
