#pragma once

#include "lo_semantics/manager_init_context.h"
#include "lo_semantics/object_refresh_curve_chain.h"

namespace lo::semantic::gpu::object_refresh_manager_curve_chain
{

struct Dependencies
{
    object_refresh_curve_chain::Dependencies curve;
    manager_init_context::PpcBoundaryServices& manager;
};

// Validation-only composition of the accepted refresh/curve/metadata path
// with actual 827C5F38 manager initialization. Its direct imports and
// dynamic guest targets remain mutable selected-context boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_curve_sample::Registers& state);

} // namespace lo::semantic::gpu::object_refresh_manager_curve_chain
