#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

class ManagerConstructionServices
{
public:
    virtual ~ManagerConstructionServices() = default;
    virtual void InitializeCriticalSection(std::uint64_t address_register) = 0;
};

// 827C5970. This leaf uses its caller's r1-16 as an observable temporary.
// It preserves the full incoming r3 register as its return value.
[[nodiscard]] std::uint64_t ConstructPrimaryManager(GuestMemory& memory,
    std::uint64_t allocation_register, GuestAddress stack_pointer);

// 827C4ED0. frame_base is the post-prologue 128-byte guest frame. The
// frame+80 and frame+148 writes are observable across the kernel callback.
[[nodiscard]] std::uint64_t ConstructFallbackManager(GuestMemory& memory,
    ManagerConstructionServices& services, std::uint64_t allocation_register,
    std::uint64_t current_manager_register, GuestAddress frame_base);

} // namespace lo::semantic::gpu
