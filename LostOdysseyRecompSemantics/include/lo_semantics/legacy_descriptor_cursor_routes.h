#pragma once
#include "lo_semantics/memory_copy_context.h"
namespace lo::semantic::gpu::legacy_descriptor_cursor_routes
{
using Registers = memory_copy_context::Registers;
struct GuestBoundaryServices
{
    virtual ~GuestBoundaryServices() = default;
    // Unselected next-frame, stack-growth and diagnostic guest callees.
    virtual void Call(GuestAddress entry, GuestMemory&, Registers&) = 0;
};
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory&, GuestBoundaryServices&, Registers&);
}
