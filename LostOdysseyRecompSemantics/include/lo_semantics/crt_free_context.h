#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_free_context
{
using Registers = crt_stream_operations::Registers;

class LowerCalls
{
public:
    virtual ~LowerCalls() = default;
    // 823ADE28 and error helpers retain mutable selected guest context.
    // Services may invoke accepted lower algorithms, including heap release.
    virtual void Call(GuestAddress entry, GuestMemory& memory,
        Registers& registers) = 0;
};

// Full 823ADDC0 caller instruction flow, including prologue/epilogue, CR0
// compare and the zero-payload path. Heap/error lower ABI remains explicit.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    LowerCalls& lower, Registers& registers);
} // namespace lo::semantic::gpu::crt_free_context
