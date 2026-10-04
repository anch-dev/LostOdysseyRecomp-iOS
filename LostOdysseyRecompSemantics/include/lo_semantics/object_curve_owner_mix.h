#pragma once

#include "lo_semantics/object_blended_curve_apply.h"

namespace lo::semantic::gpu::object_curve_owner_mix
{
using Registers = object_blended_curve_apply::Registers;
using NativeServices = object_blended_curve_apply::NativeServices;

// Actual 82625C40 single-record owner/curve blend. All direct calls use
// accepted PPC-backed lower implementations. Singleton cold construction is
// outside this selected ABI; the vtable+268 guest target remains mutable.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);
} // namespace lo::semantic::gpu::object_curve_owner_mix
