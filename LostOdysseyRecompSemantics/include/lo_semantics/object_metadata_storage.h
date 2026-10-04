#pragma once
#include "lo_semantics/object_child_float.h"
namespace lo::semantic::gpu::object_metadata_storage
{
using Registers = object_child_float::Registers;
class PpcBoundaryServices
{
public:
    virtual ~PpcBoundaryServices() = default;
    virtual void InitializeManager827C5F38(GuestMemory&, Registers&) = 0;
    virtual void CallAllocator(GuestAddress, GuestMemory&, Registers&) = 0;
};
// 825F41E8 appends a word, 822C42D8 adjusts count/capacity, and 8229F678
// dispatches the actual reallocation call and stores its returned pointer.
// Unknown manager initialization and allocator internals remain boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    PpcBoundaryServices& services, Registers& state);
}
