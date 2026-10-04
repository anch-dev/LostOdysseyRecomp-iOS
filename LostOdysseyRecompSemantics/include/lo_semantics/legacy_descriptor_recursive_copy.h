#pragma once

#include "lo_semantics/legacy_descriptor_value_intern.h"

namespace lo::semantic::gpu::legacy_descriptor_recursive_copy
{
using Registers = legacy_descriptor_value_intern::Registers;

class GuestBoundaryServices
{
public:
    virtual ~GuestBoundaryServices() = default;
    // These unrecovered guest functions receive a mutable selected PPC context.
    virtual void Call(GuestAddress entry, GuestMemory& memory,
        Registers& registers) = 0;
};

struct Dependencies
{
    legacy_descriptor_array_allocation::Services& selected;
    GuestBoundaryServices& guest;
};

// Full 83026C80 type dispatch and recursive control flow. Accepted clone and
// value-intern bodies compose directly; 83025E68, 830574D0, 8305B568 and
// diagnostic 82F99F48 remain explicit mutable guest boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_recursive_copy
