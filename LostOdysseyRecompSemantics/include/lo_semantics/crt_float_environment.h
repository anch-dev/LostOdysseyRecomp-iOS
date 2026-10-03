#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::crt_float_environment
{
using Registers = crt_stream_operations::Registers;

class NativeServices
{
public:
    virtual ~NativeServices() = default;
    virtual void CallDebugMonitor(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
    virtual void CallExceptionHandler(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
    virtual void BugCheck(GuestMemory& memory, Registers& registers) = 0;
};

// Three recovered PPC helpers. Callback targets are read from live guest RAM.
// Unknown entries leave the selected registers, RAM and native state intact.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& registers);
} // namespace lo::semantic::gpu::crt_float_environment
