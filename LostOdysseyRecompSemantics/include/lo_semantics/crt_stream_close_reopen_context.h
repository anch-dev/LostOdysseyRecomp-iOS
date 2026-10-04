#pragma once

#include "lo_semantics/crt_stream_close_file_lower.h"

namespace lo::semantic::gpu::crt_stream_close_reopen_context
{
using Registers = crt_stream_close_file_lower::Registers;
using Dependencies = crt_stream_close_file_lower::Dependencies;

// Actual DF1EA8 and DF1DD0 reopen/close routes plus the DF1F7C cleanup
// funclet. The two catalog entries connect to the already accepted DF1AA0
// and DF44A8 bodies; DF1F7C is catalog-external validation only.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);

// Validation-only composition for accepted direct callees of these bodies.
[[nodiscard]] bool ApplyAcceptedLower(GuestAddress entry,
    GuestMemory& memory, Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_close_reopen_context
