#include "lo_semantics/heap_growth.h"

namespace lo::semantic::gpu
{

GuestAddress GrowHeap(GuestMemory& memory, HeapGrowthServices& services,
    GuestAddress heap, std::uint32_t rounded_bytes, GuestAddress frame_base)
{
    const std::uint32_t requested_plus_page = rounded_bytes + 0x10000u;
    const std::uint32_t requested_pages = (requested_plus_page - 1u) >> 16;
    memory.WriteU32(frame_base + 88, requested_pages << 16);

    std::uint32_t first_empty_slot = 64;
    for (std::uint32_t slot = 0; slot < 64; ++slot)
    {
        const GuestAddress segment = memory.ReadU32(heap + (slot + 24) * 4);
        memory.WriteU32(frame_base + 84, segment);
        if (segment == 0)
        {
            if (first_empty_slot == 64)
                first_empty_slot = slot;
            continue;
        }

        if (requested_pages > memory.ReadU32(segment + 48))
            continue;
        if (memory.ReadU32(frame_base + 88) > memory.ReadU32(segment + 28))
            continue;

        const GuestAddress block = services.ExtendSegment(heap, segment,
                                                          frame_base + 88, 0);
        if (block == 0)
            continue;

        memory.WriteU32(frame_base + 88, memory.ReadU32(frame_base + 88) >> 4);
        const GuestAddress merged = CoalesceFreeBlocks(memory, services, heap,
                                                        block, frame_base + 88, 0);
        InsertFreeBlocks(memory, heap, merged, memory.ReadU32(frame_base + 88));
        return merged;
    }

    if (first_empty_slot == 64 || (memory.ReadU32(heap + 20) & 2) == 0)
        return 0;

    const std::uint32_t preferred_reserve = memory.ReadU32(heap + 32);
    memory.WriteU32(frame_base + 80, requested_plus_page);
    memory.WriteU32(frame_base + 84, 0);
    if (requested_plus_page <= preferred_reserve)
        memory.WriteU32(frame_base + 80, preferred_reserve);

    std::int32_t status = services.AllocateVirtualMemory(frame_base + 84,
        frame_base + 80, 0x60002000, 4, 0);
    while (status < 0)
    {
        const std::uint32_t attempted_size = memory.ReadU32(frame_base + 80);
        if (attempted_size == requested_plus_page)
            return 0;
        const std::uint32_t halved = attempted_size >> 1;
        memory.WriteU32(frame_base + 80, halved);
        if (halved < requested_plus_page)
            memory.WriteU32(frame_base + 80, requested_plus_page);
        status = services.AllocateVirtualMemory(frame_base + 84,
            frame_base + 80, 0x60002000, 4, 0);
    }

    const std::uint32_t committed_total = memory.ReadU32(heap + 32);
    const std::uint32_t reserved_size = memory.ReadU32(frame_base + 80);
    const std::uint32_t commit_limit = memory.ReadU32(heap + 36);
    memory.WriteU32(frame_base + 92, requested_plus_page);
    memory.WriteU32(heap + 32, committed_total + reserved_size);
    if (requested_plus_page <= commit_limit)
        memory.WriteU32(frame_base + 92, commit_limit);

    status = services.AllocateVirtualMemory(frame_base + 84,
        frame_base + 92, 0x60001000, 4, 0);
    if (status >= 0)
    {
        const GuestAddress base = memory.ReadU32(frame_base + 84);
        const std::uint32_t reserve_bytes = memory.ReadU32(frame_base + 80);
        const std::uint32_t commit_bytes = memory.ReadU32(frame_base + 92);
        const GuestAddress initialized = services.InitializeSegment(heap, base,
            first_empty_slot, 0, base, base + commit_bytes, base + reserve_bytes);
        if (initialized == 0)
            status = static_cast<std::int32_t>(0xc0000017u);
        if (status >= 0)
            return memory.ReadU32(memory.ReadU32(frame_base + 84) + 40);
    }

    services.FreeVirtualMemory(frame_base + 84, frame_base + 80, 0x8000, 0);
    return 0;
}

} // namespace lo::semantic::gpu
