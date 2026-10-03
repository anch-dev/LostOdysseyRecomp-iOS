#pragma once

#include "lo_semantics/crt_stream_operations.h"

namespace lo::semantic::gpu::object_range_destruction
{
using Registers = crt_stream_operations::Registers;

class DynamicServices
{
public:
    virtual ~DynamicServices() = default;
    virtual void CallGuestDestructor(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};

// Destroy objects in a half-open guest range. Each entry has its original
// object stride; each destructor target is read from the current live vtable.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    DynamicServices& dynamic, Registers& registers);
} // namespace lo::semantic::gpu::object_range_destruction
