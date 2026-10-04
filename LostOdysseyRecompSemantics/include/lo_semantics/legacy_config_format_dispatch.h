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

class ReallocateCalls
{
public:
    virtual ~ReallocateCalls() = default;
    virtual void Call(GuestMemory& memory, Registers& registers) = 0;
};

struct Dependencies
{
    heap_reallocate::Services& heap;
    CrtAllocationServices& allocation;
    crt_formatter::Dependencies formatter;
    crt_free_context::LowerCalls& free_lower;
    VirtualCalls& virtual_calls;
    ReallocateCalls* reallocate = nullptr;
};

// Complete 824790A8 caller body with selected-context formatter/free and
// an injectable selected-context reallocator. The default dependency retains
// the accepted exact-size large-list heap ABI and typed lower bounds.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_config_format_dispatch
