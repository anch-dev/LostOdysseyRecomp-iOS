#pragma once
#include "lo_semantics/crt_stream_operations.h"
namespace lo::semantic::gpu::memory_copy_context
{
struct Registers
{
    crt_stream_operations::Registers integer{};
    crt_stream_operations::Condition cr1{}, cr7{};
};
// Complete forward copy including caller-visible scratch and condition fields.
// Prefetch has no ordinary-RAM effect; hardware/cache/fault semantics remain open.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state);
}
