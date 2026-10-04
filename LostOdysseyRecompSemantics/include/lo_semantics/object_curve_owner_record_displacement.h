#pragma once

#include "lo_semantics/object_curve_record_displacement.h"
#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::object_curve_owner_record_displacement
{
using Registers = object_curve_record_displacement::Registers;
using NativeServices = object_curve_record_displacement::NativeServices;

// Actual 82627C98 owner-blended indexed-record displacement. The direct
// 8229F208 normalizer is supplied by object_curve_record_displacement and
// guest vtable+268 calls remain mutable selected-register boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);
} // namespace lo::semantic::gpu::object_curve_owner_record_displacement
