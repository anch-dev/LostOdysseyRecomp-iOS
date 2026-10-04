#pragma once

#include "lo_semantics/object_curve_pair_apply.h"
#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::object_blended_curve_apply
{
struct Registers : object_curve_pair_apply::Registers
{
    std::uint64_t f2_bits = 0, f7_bits = 0, f8_bits = 0, f9_bits = 0;
    std::uint64_t f29_bits = 0;
};

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void SetHostFpControl(std::uint32_t control) = 0;
    virtual void CallVirtual(GuestAddress target, GuestMemory& memory,
        Registers& state) = 0;
};

// Actual 82625E90 blend/update and 8262C430 owner selection, with accepted
// 8242CC78 singleton, 82607318 selector and curve lower bodies. The
// singleton must be warm; a cold call returns false without changing state.
// Dynamic vtable+268 targets retain the full selected extended FPR context.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);
} // namespace lo::semantic::gpu::object_blended_curve_apply
