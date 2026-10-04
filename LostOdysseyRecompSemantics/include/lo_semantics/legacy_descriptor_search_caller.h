#pragma once

#include "lo_semantics/legacy_descriptor_search_helpers.h"

namespace lo::semantic::gpu::legacy_descriptor_search_caller
{
using Registers = crt_stream_operations::Registers;

class DiagnosticServices
{
public:
    virtual ~DiagnosticServices() = default;
    // 82F99F48 spills its own frame, then tails into 82F99D98. The latter
    // receives and may mutate the selected guest context and ordinary RAM.
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

// Full 83058FC8 selected control flow, composing accepted descriptor layout
// and search helpers. The diagnostic continuation is explicit and mutable.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    DiagnosticServices& diagnostic, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_search_caller
