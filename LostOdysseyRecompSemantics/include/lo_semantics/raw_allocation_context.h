#pragma once
#include "lo_semantics/object_child_float.h"
namespace lo::semantic::gpu::raw_allocation_context
{
struct Registers : object_child_float::Registers
{ object_child_float::Condition cr0{}; };
class PpcBoundaryServices
{
public:
    virtual ~PpcBoundaryServices() = default;
    virtual void CallDirect(GuestAddress, GuestMemory&, Registers&) = 0;
};
// Actual 823ACBD0 and 823ACC98 selected ABI, including full frame/register
// state and caller-visible heap/CRT boundary effects. Retry is not bounded.
[[nodiscard]] bool Apply(GuestAddress, GuestMemory&, PpcBoundaryServices&,
    Registers&);
}
