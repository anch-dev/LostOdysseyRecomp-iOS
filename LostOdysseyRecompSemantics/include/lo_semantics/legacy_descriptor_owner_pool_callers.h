#pragma once

#include "lo_semantics/legacy_descriptor_value_intern.h"
#include "lo_semantics/heap_allocation_context.h"

namespace lo::semantic::gpu::legacy_descriptor_owner_pool_callers
{
struct Registers : legacy_descriptor_value_intern::Registers
{
    std::uint64_t f30_bits = 0, f31_bits = 0;
};

class GuestBoundaryServices
{
public:
    virtual ~GuestBoundaryServices() = default;
    // The still-unrecovered guest 82FAC428 and physical allocation path may
    // mutate the selected PPC context and guest RAM.
    virtual void Call(GuestAddress target, GuestMemory& memory,
        Registers& registers) = 0;
};
struct Dependencies
{
    GuestBoundaryServices& guest;
    heap_allocation_context::BoundaryServices& heap;
};

// Complete owner-pool allocator and large-block list linker. The accepted
// 82B7BC40 fill and the accepted heap allocation context are composed.
// 827C9D88's general dispatch and 827CAD38 wrapper retain selected context;
// unrecovered guest callees stay explicit. The heap service itself owns its
// documented guest and actual kernel/import boundaries.
[[nodiscard]] bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers);
} // namespace lo::semantic::gpu::legacy_descriptor_owner_pool_callers
