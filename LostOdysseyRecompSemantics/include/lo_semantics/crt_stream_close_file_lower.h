#pragma once

#include "lo_semantics/crt_stream_close_shared_lower.h"

namespace lo::semantic::gpu::crt_stream_close_file_lower
{
using Registers = crt_stream_close_shared_lower::Registers;
using Dependencies = crt_stream_close_shared_lower::Dependencies;

// Five exact PPC bodies close the DF1CD0 -> DF1AA0 -> DF44A8 chain.
// DF1D94 and DF4600 are catalog-external cleanup funclets used only for
// original-body validation. Accepted stream, pointer unlock and open-position
// callees keep their selected guest/native service boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Validation-only adapter for this chain's already accepted direct callees.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry,
    GuestMemory& memory, Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_close_file_lower
