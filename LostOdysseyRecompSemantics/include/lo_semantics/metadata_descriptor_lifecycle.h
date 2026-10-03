#pragma once

#include "lo_semantics/manager_facade.h"

#include <cstdint>

namespace lo::semantic::gpu::metadata_descriptor_lifecycle
{
struct VolatileRegisters
{
    std::uint64_t r3 = 0, r4 = 0, r5 = 0;
    std::uint64_t r6 = 0, r7 = 0, r8 = 0;
};

struct FrameRegisters
{
    std::uint64_t lr = 0;
    std::uint64_t r26 = 0, r27 = 0, r28 = 0;
    std::uint64_t r29 = 0, r30 = 0, r31 = 0;
    std::uint64_t ctr = 0;
};

// 8240EFC0 dispatches each node through its vtable+360 method. The service
// receives the masked target, full live r3-r8/SP/LR/CTR/r26-r31, and mutable
// guest RAM. It is a boundary for the dynamic target implementation.
class VirtualServices
{
public:
    virtual ~VirtualServices() = default;
    virtual void Dispatch(GuestAddress target, GuestMemory& memory,
        VolatileRegisters& registers, std::uint64_t caller_sp,
        FrameRegisters& frame) = 0;
};

// 82401C58 releases its object and two embedded arrays through the accepted
// manager and array algorithms. Own saves and the immediate lower functions'
// known guest frames are included. Generic lower ABI effects outside those
// APIs, deeper saved-slot redirects, callback internals, MMIO and unbounded
// node lists remain outside this selected ordinary-RAM contract.
// Unknown addresses leave all inputs, memory and services untouched.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager, VirtualServices& methods,
    VolatileRegisters& registers, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result);
} // namespace lo::semantic::gpu::metadata_descriptor_lifecycle
