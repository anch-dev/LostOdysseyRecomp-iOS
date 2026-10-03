#pragma once

#include "lo_semantics/crt_allocation.h"
#include "lo_semantics/crt_float_formatting.h"
#include "lo_semantics/crt_wide_stream_output.h"

namespace lo::semantic::gpu::crt_formatter
{
using Registers = crt_stream_operations::Registers;

class DynamicServices
{
public:
    virtual ~DynamicServices() = default;
    // Formatter slots are mutable guest function pointers. Known installed
    // targets use recovered PPC models; a different live target is explicit.
    virtual void CallGuestFormatter(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

struct Dependencies
{
    crt_wide_stream_output::Dependencies output;
    crt_float_formatting::Dependencies floating;
    crt_formatting_support::NativeServices& support;
    CrtAllocationServices& allocation;
    DynamicServices& dynamic;
};

// CRT wide format entry, bounded and unbounded buffer wrappers, and engine.
// All format tables, float slots, strings and varargs remain live guest data.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::crt_formatter
