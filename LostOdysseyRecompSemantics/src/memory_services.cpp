#include "lo_semantics/memory_services.h"

#include <bit>

namespace lo::semantic::gpu
{

GuestAddress AllocatePhysicalMemory(GuestMemory& memory, KernelMemoryServices& services,
    std::uint32_t bytes, GuestAddress requested, std::uint32_t alignment,
    std::uint32_t protect)
{
    if (memory.ReadU32(0x83247200) != 0)
        protect &= ~0x20000000u;

    GuestAddress minimum = 0;
    GuestAddress maximum = 0xffffffffu;
    if (requested != 0xffffffffu)
    {
        minimum = requested;
        maximum = requested + bytes - 1;
        alignment = 0;
    }

    const GuestAddress result = services.AllocatePhysical(
        0, bytes, protect, minimum, maximum, alignment);
    if (result == 0)
        services.ReportAllocationFailure(8);
    return result;
}

void FreePhysicalMemory(KernelMemoryServices& services, GuestAddress address)
{
    services.FreePhysical(0, address);
}

GuestAddress GetProcessHeap(GuestMemory& memory)
{
    return memory.ReadU32(0x83245708);
}

GuestAddress AllocateHeapMemory(GuestMemory& memory, KernelMemoryServices& services,
    std::uint32_t flags, std::uint32_t bytes)
{
    const GuestAddress heap = GetProcessHeap(memory);
    return services.AllocateHeap(heap, std::rotl(flags, 29) & 8u, bytes);
}

GuestAddress FreeHeapMemory(GuestMemory& memory, KernelMemoryServices& services,
    GuestAddress address)
{
    const GuestAddress heap = GetProcessHeap(memory);
    return services.FreeHeap(heap, 0, address) != 0 ? 0 : address;
}

} // namespace lo::semantic::gpu
