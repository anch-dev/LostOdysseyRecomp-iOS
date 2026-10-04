#pragma once
#include "lo_semantics/raw_allocation_context.h"
namespace lo::semantic::gpu::manager_init_raw_allocation_chain
{
using Registers = raw_allocation_context::Registers;
class PpcBoundaryServices : public raw_allocation_context::PpcBoundaryServices
{
public:
    virtual void CallVirtual(GuestAddress, GuestMemory&, Registers&) = 0;
};
// Actual manager initialization calls the recovered raw allocator and getter.
// Heap allocation, constructors and dynamic methods remain mutable boundaries.
void Apply(GuestMemory&, PpcBoundaryServices&, Registers&);
}
