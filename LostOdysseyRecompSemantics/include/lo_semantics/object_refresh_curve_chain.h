#pragma once

#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/object_refresh_metadata_chain.h"

namespace lo::semantic::gpu::object_refresh_curve_chain
{

struct Dependencies
{
    object_refresh_metadata_chain::Dependencies refresh;
    object_curve_sample::NativeServices& curve;
};

// Validation-only composition: actual refresh, metadata, child, and curve
// PPC lowers. Curve f10/f11/f12 are explicit selected-state extensions to
// the refresh ABI; dynamic guest targets remain mutable boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_curve_sample::Registers& state);

} // namespace lo::semantic::gpu::object_refresh_curve_chain
