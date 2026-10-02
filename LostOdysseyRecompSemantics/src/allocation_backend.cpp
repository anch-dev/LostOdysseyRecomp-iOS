#include "lo_semantics/allocation_backend.h"

#include <bit>

namespace lo::semantic::gpu
{

GuestAddress AllocateGeneral(GuestMemory& memory, GeneralAllocationServices& services,
    std::uint32_t bytes, std::uint32_t flags)
{
    if ((flags & 0x80000000u) == 0)
        return services.AllocateHeap(std::rotl(flags, 8) & 0x40u, bytes);

    if ((flags & 0x0f000000u) == 0)
        flags |= 0x0c000000u;
    const std::uint32_t alignment = 1u << (std::rotl(flags, 8) & 0xfu);
    const GuestAddress policy_address = 0x831e7824u + (std::rotl(flags, 6) & 0xcu);
    const std::uint32_t protection = memory.ReadU32(policy_address);
    const GuestAddress result = services.AllocatePhysical(bytes, 0xffffffffu, alignment, protection);
    if (result != 0 && (flags & 0x40000000u) != 0)
        services.Fill(result, 0, bytes);
    return result;
}

std::uint32_t FreeGeneral(GeneralAllocationServices& services, GuestAddress address,
    std::uint32_t flags)
{
    if ((flags & 0x80000000u) != 0)
        return address != 0 ? services.FreePhysical(address) : 0;
    return services.FreeHeap(address);
}

} // namespace lo::semantic::gpu
