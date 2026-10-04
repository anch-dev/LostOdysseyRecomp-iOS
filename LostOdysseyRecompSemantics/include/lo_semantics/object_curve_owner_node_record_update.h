#pragma once

#include "lo_semantics/object_curve_record_displacement.h"
#include "lo_semantics/registered_constructor_family.h"

namespace lo::semantic::gpu::object_curve_owner_node_record_update
{
using Registers = object_curve_record_displacement::Registers;
using NativeServices = object_curve_record_displacement::NativeServices;

// Actual 82628E50 owner-blended node lookup and indexed-record update.
// Direct curve/normalization and the warm singleton reuse accepted bodies;
// guest vtable+116 remains a mutable selected-register boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state);
} // namespace lo::semantic::gpu::object_curve_owner_node_record_update
