#pragma once

#include "lo_semantics/guest_memory.h"

namespace lo::semantic::gpu
{

// The kernel imports and the guest last-error/heap routines remain service
// boundaries. No host pointer is inferred from a 32-bit guest address.
class KernelMemoryServices
{
public:
    virtual ~KernelMemoryServices() = default;
    virtual GuestAddress AllocatePhysical(std::uint32_t type, std::uint32_t bytes,
        std::uint32_t protect, GuestAddress minimum, GuestAddress maximum,
        std::uint32_t alignment) = 0;
    virtual void FreePhysical(std::uint32_t type, GuestAddress address) = 0;
    virtual void ReportAllocationFailure(std::uint32_t code) = 0;
    virtual GuestAddress AllocateHeap(GuestAddress heap, std::uint32_t flags,
        std::uint32_t bytes) = 0;
    virtual std::uint32_t FreeHeap(GuestAddress heap, std::uint32_t flags,
        GuestAddress address) = 0;
};

// Guest functions 827C9E20, 827C9EB8, 827CAD38, 827CAD80 and 823ACC98.
[[nodiscard]] GuestAddress AllocatePhysicalMemory(GuestMemory& memory,
    KernelMemoryServices& services, std::uint32_t bytes, GuestAddress requested,
    std::uint32_t alignment, std::uint32_t protect);

// MmFreePhysicalMemory is a void kernel import. Its residual r3 value is not
// exposed as a defined return value by this semantic API.
void FreePhysicalMemory(KernelMemoryServices& services, GuestAddress address);

[[nodiscard]] GuestAddress GetProcessHeap(GuestMemory& memory);
[[nodiscard]] GuestAddress AllocateHeapMemory(GuestMemory& memory,
    KernelMemoryServices& services, std::uint32_t flags, std::uint32_t bytes);
[[nodiscard]] GuestAddress FreeHeapMemory(GuestMemory& memory,
    KernelMemoryServices& services, GuestAddress address);

} // namespace lo::semantic::gpu
