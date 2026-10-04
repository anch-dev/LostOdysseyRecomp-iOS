#pragma once

#include "lo_semantics/crt_stream_close_error.h"

namespace lo::semantic::gpu::crt_stream_close_caller
{
using Registers = crt_stream_operations::Registers;
using Dependencies = crt_stream_close_error::Dependencies;

// 82B87B18 performs the locked close, and 82B87C54 releases its lock through
// the accepted 82B863F0 guest leaf/native import. Both complete generated
// bodies retain live selected GPR/CR/XER/SP/LR/CTR and ordered guest RAM.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_close_caller
