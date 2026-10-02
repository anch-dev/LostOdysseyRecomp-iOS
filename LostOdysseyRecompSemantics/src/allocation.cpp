#include "lo_semantics/allocation.h"

namespace lo::semantic::gpu
{
namespace
{
constexpr std::uint32_t special_flags = 0xa7820007;
constexpr GuestAddress special_allocator = 0x832471a8;
}

GuestAddress AllocateDispatch(AllocationServices& services, std::uint32_t bytes,
    std::uint32_t flags)
{
    if (flags == special_flags)
        return services.AllocateSpecial(special_allocator, bytes);
    return services.AllocateGeneral(bytes, flags);
}

std::uint32_t FreeDispatch(AllocationServices& services, GuestAddress address,
    std::uint32_t flags)
{
    if (flags == special_flags)
        return services.FreeSpecial(special_allocator, address);
    return services.FreeGeneral(address, flags);
}

} // namespace lo::semantic::gpu
