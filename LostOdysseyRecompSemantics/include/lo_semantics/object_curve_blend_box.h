#pragma once

#include "lo_semantics/object_curve_box_dispatch.h"
#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::object_curve_blend_box
{
using Registers = object_curve_box_dispatch::Registers;
using NativeServices = object_curve_box_dispatch::NativeServices;

// Actual 826266D0 four-vector blend and box dispatch. All direct curve and
// selector calls use accepted PPC-backed lower bodies. The 8242CDF8 lazy
// singleton is selected only with an existing singleton; its cold allocation
// and post-callback ABI are outside this entry's selected contract.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);
} // namespace lo::semantic::gpu::object_curve_blend_box
