#pragma once

#include "lo_semantics/crt_stream_open_pipeline.h"
#include "lo_semantics/crt_stream_pointer_unlock.h"

namespace lo::semantic::gpu::crt_stream_open_wrapper
{
using Registers = crt_stream_open_pipeline::Registers;

struct Dependencies
{
    crt_stream_open_pipeline::Dependencies open;
    crt_stream_pointer_unlock::NativeServices& unlock;
};

// 82DF6760 is the public open wrapper. 82DF6868 and 82DF6888 have distinct
// entry prologues but share the same cleanup tail and accepted unlock leaf.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state);
} // namespace lo::semantic::gpu::crt_stream_open_wrapper
