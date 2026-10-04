#pragma once
#include "lo_semantics/object_child_float.h"
namespace lo::semantic::gpu::manager_init_context
{
using Registers = object_child_float::Registers;
class PpcBoundaryServices
{
public:
    virtual ~PpcBoundaryServices() = default;
    virtual void CallDirect(GuestAddress, GuestMemory&, Registers&) = 0;
    virtual void CallVirtual(GuestAddress, GuestMemory&, Registers&) = 0;
};
// 827C5F38 selected-context adapter, including its frame and live registers.
// Allocation, constructors and dynamically selected guests remain boundaries.
void Apply(GuestMemory&, PpcBoundaryServices&, Registers&);
}
