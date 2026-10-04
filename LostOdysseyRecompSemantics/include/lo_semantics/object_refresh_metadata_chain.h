#pragma once

#include "lo_semantics/object_metadata_storage.h"
#include "lo_semantics/object_refresh_routes.h"

namespace lo::semantic::gpu::object_refresh_metadata_chain
{

struct Dependencies
{
    object_child_float_record_chain::Dependencies child_chain;
    object_refresh_routes::PpcBoundaryServices& remaining;
    object_metadata_storage::PpcBoundaryServices& metadata;
};

// Validation-only composition of 826099A8/8260A3F0 with the actual
// 825F41E8 -> 822C42D8 -> 8229F678 metadata storage path. 822C7388 and
// guest vtable calls remain selected-context boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state);

} // namespace lo::semantic::gpu::object_refresh_metadata_chain
