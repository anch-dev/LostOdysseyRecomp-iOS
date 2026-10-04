#pragma once

#include "lo_semantics/object_parent_flag_probe.h"

namespace lo::semantic::gpu::object_owner_flag_routes
{

struct Dependencies
{
    object_parent_flag_probe::Dependencies probe;
};

// 8236B4D0 combines the +128 owner route, 8236B578's FP/flag selector,
// the parent flag probe and the live vtable+376 guest boundary. The probe is
// a separate pending recovery dependency; its acceptance is not presumed.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state);

} // namespace lo::semantic::gpu::object_owner_flag_routes
