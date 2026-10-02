#include "lo_semantics/heap_ranges.h"

namespace lo::semantic::gpu
{

GuestAddress AllocateRangeNode(GuestMemory& memory, HeapRangeServices& services,
    GuestAddress segment, GuestAddress frame_base)
{
    const GuestAddress arena = memory.ReadU32(segment + 24);
    const GuestAddress free_head = arena + 76;
    if (memory.ReadU32(free_head) == 0)
    {
        const GuestAddress reserved = memory.ReadU32(arena + 72);
        memory.WriteU32(frame_base + 80, reserved);
        if (reserved != 0 &&
            memory.ReadU32(reserved + 8) != memory.ReadU32(reserved + 4))
        {
            memory.WriteU32(frame_base + 84, 0x10000);
            memory.WriteU32(frame_base + 92,
                            reserved + memory.ReadU32(reserved + 8));
            if (services.AllocateVirtualMemory(frame_base + 92,
                                               frame_base + 84,
                                               0x60001000, 4, 0) < 0)
                return 0;

            const GuestAddress current_reservation = memory.ReadU32(frame_base + 80);
            memory.WriteU32(current_reservation + 8,
                            memory.ReadU32(current_reservation + 8) +
                            memory.ReadU32(frame_base + 84));
        }
        else
        {
            memory.WriteU32(frame_base + 80, 0);
            memory.WriteU32(frame_base + 88, 0x100000);
            if (services.AllocateVirtualMemory(frame_base + 80,
                                               frame_base + 88,
                                               0x60002000, 4, 0) < 0)
                return 0;

            memory.WriteU32(frame_base + 84, 0x10000);
            if (services.AllocateVirtualMemory(frame_base + 80,
                                               frame_base + 84,
                                               0x60001000, 4, 0) < 0)
            {
                (void)services.FreeVirtualMemory(frame_base + 80,
                                                 frame_base + 88, 0x8000, 0);
                return 0;
            }

            const GuestAddress new_reservation = memory.ReadU32(frame_base + 80);
            const GuestAddress current_arena = memory.ReadU32(segment + 24);
            memory.WriteU32(new_reservation, memory.ReadU32(current_arena + 72));
            memory.WriteU32(memory.ReadU32(segment + 24) + 72,
                            memory.ReadU32(frame_base + 80));
            memory.WriteU32(memory.ReadU32(frame_base + 80) + 4,
                            memory.ReadU32(frame_base + 88));
            memory.WriteU32(memory.ReadU32(frame_base + 80) + 8,
                            memory.ReadU32(frame_base + 84));
            memory.WriteU32(frame_base + 92, memory.ReadU32(frame_base + 80) + 16);
        }

        const GuestAddress reservation = memory.ReadU32(frame_base + 80);
        const GuestAddress end = reservation + memory.ReadU32(reservation + 8);
        const GuestAddress list_head = memory.ReadU32(segment + 24) + 76;
        GuestAddress link = list_head;
        GuestAddress cursor = memory.ReadU32(frame_base + 92);
        while (cursor < end)
        {
            memory.WriteU32(link, cursor);
            link = cursor;
            cursor += 16;
        }
        memory.WriteU32(link, 0);
    }

    const GuestAddress current_head = memory.ReadU32(memory.ReadU32(segment + 24) + 76);
    const GuestAddress next = memory.ReadU32(current_head);
    memory.WriteU32(memory.ReadU32(segment + 24) + 76, next);
    return current_head;
}

void InsertRangeRecord(GuestMemory& memory, HeapRangeServices& services,
    GuestAddress segment, GuestAddress base, std::uint32_t bytes,
    GuestAddress frame_base)
{
    GuestAddress link = segment + 56;
    GuestAddress current = memory.ReadU32(link);
    while (current != 0)
    {
        const GuestAddress current_base = memory.ReadU32(current + 4);
        if (current_base > base)
        {
            if (base + bytes == current_base)
            {
                const std::uint32_t merged = bytes + memory.ReadU32(current + 8);
                memory.WriteU32(current + 4, base);
                memory.WriteU32(current + 8, merged);
                if (merged > memory.ReadU32(segment + 28))
                    memory.WriteU32(segment + 28, merged);
                return;
            }
            break;
        }

        const std::uint32_t current_bytes = memory.ReadU32(current + 8);
        if (current_base + current_bytes == base)
        {
            bytes += current_bytes;
            const GuestAddress next = memory.ReadU32(current);
            base = current_base;
            memory.WriteU32(link, next);

            const GuestAddress arena = memory.ReadU32(segment + 24);
            memory.WriteU32(current, memory.ReadU32(arena + 76));
            memory.WriteU32(memory.ReadU32(segment + 24) + 76, current);
            memory.WriteU32(current + 4, 0);
            memory.WriteU32(current + 8, 0);
            memory.WriteU32(segment + 52, memory.ReadU32(segment + 52) - 1);
            if (bytes > memory.ReadU32(segment + 28))
                memory.WriteU32(segment + 28, bytes);
        }
        else
        {
            link = current;
        }
        current = memory.ReadU32(link);
    }

    const GuestAddress node = AllocateRangeNode(memory, services, segment,
                                                 frame_base - 128);
    if (node == 0)
        return;

    memory.WriteU32(node + 4, base);
    memory.WriteU32(node + 8, bytes);
    memory.WriteU32(node, memory.ReadU32(link));
    memory.WriteU32(link, node);
    memory.WriteU32(segment + 52, memory.ReadU32(segment + 52) + 1);
    if (bytes >= memory.ReadU32(segment + 28))
        memory.WriteU32(segment + 28, bytes);
}

} // namespace lo::semantic::gpu
