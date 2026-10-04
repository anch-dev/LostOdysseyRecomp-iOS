#pragma once

#include "lo_semantics/object_curve_record_displacement.h"
#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::object_curve_owner_vector_record
{
using Registers = object_curve_record_displacement::Registers;
using NativeServices = object_curve_record_displacement::NativeServices;

// Actual 826297E0 owner-blended vector and indexed-record update. All direct
// lower calls reuse accepted bodies; nested vtable+268 remains mutable.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);
} // namespace lo::semantic::gpu::object_curve_owner_vector_record
