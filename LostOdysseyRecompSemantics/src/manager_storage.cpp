#include "lo_semantics/manager_storage.h"

namespace lo::semantic::gpu
{

std::uint64_t InitializeStorageBuckets(GuestMemory& memory,
    std::uint64_t base_register)
{
    const GuestAddress base = static_cast<GuestAddress>(base_register);
    const GuestAddress head = base + 0x28000u;
    memory.WriteU32(head, 0);

    for (std::uint32_t index = 0; index < 8192; ++index)
    {
        const GuestAddress entry = base + index * 20u;
        const GuestAddress links = entry + 12u;
        memory.WriteU32(entry, 0);
        memory.WriteU32(entry + 4u, 0);
        memory.WriteU32(entry + 8u, 0);

        const GuestAddress previous = memory.ReadU32(head);
        if (previous != 0)
            memory.WriteU32(previous + 16u, links);

        // The backlink write above can alias the head; PPC reloads it.
        const GuestAddress current_head = memory.ReadU32(head);
        memory.WriteU32(links + 4u, head);
        memory.WriteU32(links, current_head);
        memory.WriteU32(head, entry);
    }
    return base_register;
}

std::uint64_t InitializePrimaryManagerStorage(GuestMemory& memory,
    std::uint64_t manager_register)
{
    const GuestAddress manager = static_cast<GuestAddress>(manager_register);
    memory.WriteU32(manager + 0x20dbcu, 1);
    memory.WriteU32(manager + 3372u, 0);
    memory.WriteU32(manager + 0x48de4u, 0);
    memory.WriteU32(manager + 0x48de8u, 0);
    memory.WriteU32(manager + 3364u, 0xffffffffu);
    memory.WriteU32(manager + 3368u, 4);
    memory.WriteU32(manager + 3376u, 0);
    memory.WriteU32(manager + 3380u, 0);

    const GuestAddress lookup = manager + 3512u;
    for (std::uint32_t group = 0; group < 4; ++group)
    {
        const GuestAddress first = manager + 8u + group * 840u;
        const std::uint32_t flag = group == 3 ? 0u : 1u;
        memory.WriteU32(first, group);
        memory.WriteU32(first + 4u, 0);
        memory.WriteU32(first + 8u, 0);
        memory.WriteU32(first + 12u, 8);
        memory.WriteU32(first - 4u, flag);

        GuestAddress bucket = first + 20u;
        for (std::uint32_t tier = 1; tier < 5; ++tier)
        {
            const std::uint32_t next_tier = tier + 1u;
            memory.WriteU32(bucket - 4u, flag);
            memory.WriteU32(bucket, group);
            memory.WriteU32(bucket + 4u, 0);
            memory.WriteU32(bucket + 8u, 0);
            const std::uint32_t size = (2u << tier) + (8u << (next_tier >> 2));
            memory.WriteU32(bucket + 12u, size);
            bucket += 20u;
        }

        bucket = first + 100u;
        for (std::uint32_t tier = 4; tier < 41; ++tier)
        {
            memory.WriteU32(bucket - 4u, flag);
            memory.WriteU32(bucket, group);
            memory.WriteU32(bucket + 4u, 0);
            memory.WriteU32(bucket + 8u, 0);
            const std::uint32_t size = ((tier & 3u) + 4u) << (((tier + 8u) >> 2) + 1u);
            memory.WriteU32(bucket + 12u, size);
            bucket += 20u;
        }

        for (std::uint32_t requested = 0; requested < 32769u; ++requested)
        {
            std::uint32_t index = 0;
            if (memory.ReadU32(first + 12u) < requested)
            {
                GuestAddress cursor = first + 12u;
                do
                {
                    cursor += 20u;
                    ++index;
                } while (memory.ReadU32(cursor) < requested);
            }
            memory.WriteU32(lookup + requested * 4u, index);
        }
    }

    for (std::uint32_t index = 0; index < 32; ++index)
        memory.WriteU32(manager + 3384u + index * 4u, 0);

    return InitializeStorageBuckets(memory, manager_register + 0x20de0u);
}

} // namespace lo::semantic::gpu
