#pragma once

#include "lo_semantics/crt_formatter.h"
#include "lo_semantics/crt_free_context.h"
#include "lo_semantics/crt_reallocate.h"

namespace lo::semantic::gpu::legacy_config_format_dispatch
{
using Registers = crt_stream_operations::Registers;

class VirtualCalls
{
public:
    virtual ~VirtualCalls() = default;
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

struct Dependencies
{
    heap_reallocate::Services& heap;
    CrtAllocationServices& allocation;
    crt_formatter::Dependencies formatter;
    crt_free_context::LowerCalls& free_lower;
    VirtualCalls& virtual_calls;
};

// Complete 824790A8 caller body with selected-context formatter/free and
// exact-size large-list heap ABI. Other heap paths retain typed lower bounds.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_config_format_dispatch
