#pragma once

#include "lo_semantics/object_curve_box_dispatch.h"
#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::object_curve_threshold_routes
{
using Registers = object_curve_box_dispatch::Registers;
using NativeServices = object_curve_box_dispatch::NativeServices;

// Actual scalar threshold routes 82626C88 and 82626E60. The owner route
// uses the accepted 8242CEB8 singleton only when its global is already set.
// Guest vtable+268/+108 targets remain mutable selected-context boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);
} // namespace lo::semantic::gpu::object_curve_threshold_routes
