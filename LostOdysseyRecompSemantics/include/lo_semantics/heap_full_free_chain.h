#pragma once

#include "lo_semantics/heap_coalesce_context.h"

namespace lo::semantic::gpu::heap_full_free_chain
{
using Registers = heap_coalesce_context::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void CallNative(GuestAddress entry, GuestMemory& memory,
        Registers& state) = 0;
};

// Composes selected 823ADE28/823AE0BC, 823AE108, 827CBA60,
// 827CC668, 827CB498 and 827CB658. Kernel imports remain explicit
// mutable boundaries; only selected integer/CR0/CR6/XER/LR/CTR/RAM state
// is represented.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state);
} // namespace lo::semantic::gpu::heap_full_free_chain
