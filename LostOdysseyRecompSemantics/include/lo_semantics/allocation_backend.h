#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

class GeneralAllocationServices
{
public:
    virtual ~GeneralAllocationServices() = default;
    virtual GuestAddress AllocatePhysical(std::uint32_t bytes, GuestAddress requested_address,
        std::uint32_t alignment, std::uint32_t protection) = 0;
    virtual GuestAddress AllocateHeap(std::uint32_t flags, std::uint32_t bytes) = 0;
    virtual void Fill(GuestAddress address, std::uint8_t value, std::uint32_t bytes) = 0;
    virtual std::uint32_t FreePhysical(GuestAddress address) = 0;
    virtual std::uint32_t FreeHeap(GuestAddress address) = 0;
};

// 827CA050 and 827CA0E8. The physical protection table is guest memory at
// 831E7824. Helper services still supply physical/heap allocation and release.
[[nodiscard]] GuestAddress AllocateGeneral(GuestMemory& memory, GeneralAllocationServices& services,
    std::uint32_t bytes, std::uint32_t flags);
std::uint32_t FreeGeneral(GeneralAllocationServices& services, GuestAddress address,
    std::uint32_t flags);

} // namespace lo::semantic::gpu
