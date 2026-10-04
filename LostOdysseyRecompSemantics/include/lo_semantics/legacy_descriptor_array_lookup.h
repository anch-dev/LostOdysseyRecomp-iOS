#pragma once

#include "lo_semantics/legacy_descriptor_array_allocation.h"

namespace lo::semantic::gpu::legacy_descriptor_array_lookup
{
using Registers = legacy_descriptor_array_allocation::Registers;

class FreeTailServices
{
public:
    virtual ~FreeTailServices() = default;
    // 827C9C60 special release, 827CAD80's 823ADE28 heap release, and
    // 827C9EB8's physical-memory import retain mutable selected context.
    virtual void Call(GuestAddress target, GuestMemory& memory,
        crt_stream_operations::Registers& integer) = 0;
};

struct Dependencies
{
    legacy_descriptor_array_allocation::Services& allocation;
    FreeTailServices& free_tail;
};

// Complete 83058AD8 descriptor lookup/intern route and 82FAC980 size-class
// return. Actual accepted constructor/selector and free-dispatch control
// flow are composed; deep heap/physical release remains a native boundary.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_array_lookup
