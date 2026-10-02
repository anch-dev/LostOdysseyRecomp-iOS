#include "lo_semantics/query.h"

namespace lo::semantic::gpu
{

GuestAddress CreateType9Query(GuestMemory& memory, QueryServices& services, GuestAddress device)
{
    constexpr std::uint32_t query_size = 0x9c;
    constexpr std::uint32_t allocation_flags = 0x64800000;
    const GuestAddress query = services.Allocate(query_size, allocation_flags);
    if (query == 0)
        return 0;

    // Preserve the store order of 827B7434..827B7454. The allocator owns the
    // remaining bytes; this function does not initialize or clear them.
    memory.WriteU32(query + 0x00, device);
    memory.WriteU32(query + 0x0c, 1);
    memory.WriteU32(query + 0x04, 9);
    memory.WriteU32(query + 0x98, 1);
    memory.WriteU32(query + 0x94, 1);

    std::uint32_t completed = 0;
    GuestAddress owner_slot = query + 0x58;
    for (;;)
    {
        const GuestAddress data_slot = owner_slot - 0x3c;
        if (services.InitializeSlot(device, owner_slot, data_slot) < 0)
        {
            memory.WriteU32(query + 0x98, completed);
            services.Release(query);
            return 0;
        }

        ++completed;
        owner_slot += 4;
        if (completed >= memory.ReadU32(query + 0x98))
            break;
    }

    memory.WriteU8(query + 0x10, 1);
    return query;
}

} // namespace lo::semantic::gpu
