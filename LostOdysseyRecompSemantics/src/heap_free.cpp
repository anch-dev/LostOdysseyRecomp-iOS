#include "lo_semantics/heap_free.h"

namespace lo::semantic::gpu
{
namespace
{

std::uint16_t ReadU16(GuestMemory& memory, GuestAddress address)
{
    return static_cast<std::uint16_t>((std::uint16_t{memory.ReadU8(address)} << 8) |
                                      memory.ReadU8(address + 1));
}

GuestAddress SmallListHead(GuestAddress heap, std::uint32_t units)
{
    return heap + (units + 48) * 8;
}

GuestAddress BitmapWord(GuestAddress heap, std::uint32_t units)
{
    return heap + ((units >> 5) + 88) * 4;
}

} // namespace

void LeaveHeapCriticalSection(GuestMemory& memory, HeapFreeServices& services,
    GuestAddress heap, std::uint32_t lock_owned)
{
    if (lock_owned != 0)
        services.LeaveCriticalSection(memory.ReadU32(heap + 1408));
}

std::uint32_t FreeHeapBlock(GuestMemory& memory, HeapFreeServices& services,
    GuestAddress heap, std::uint32_t flags, GuestAddress payload,
    GuestAddress frame_base)
{
    const std::uint32_t process_guard = memory.ReadU32(heap + 20);
    memory.WriteU32(frame_base + 100, heap);
    std::uint32_t lock_owned = 0;
    memory.WriteU32(frame_base + 84, 0);
    memory.WriteU32(frame_base + 88, 1);

    if ((process_guard & 0x40000) != 0)
    {
        const std::uint32_t current_process = services.GetCurrentProcessType();
        if (memory.ReadU8(heap + 379) != current_process)
            services.BugCheck(244, heap, memory.ReadU32(frame_base + 168),
                              4390, payload);
    }
    if (payload == 0)
        return 1;

    GuestAddress block = payload - 16;
    const std::uint32_t heap_flags = memory.ReadU32(heap + 24);
    if (((heap_flags | flags) & 1) == 0)
    {
        services.EnterCriticalSection(memory.ReadU32(heap + 1408));
        lock_owned = 1;
        memory.WriteU32(frame_base + 84, lock_owned);
    }

    if ((memory.ReadU8(block + 5) & 8) != 0)
    {
        const GuestAddress vm_base = block - 32;
        memory.WriteU32(frame_base + 96, vm_base);
        const GuestAddress next = memory.ReadU32(vm_base);
        const GuestAddress previous = memory.ReadU32(vm_base + 4);
        memory.WriteU32(previous, next);
        memory.WriteU32(next + 4, previous);

        if (lock_owned != 0)
        {
            services.LeaveCriticalSection(memory.ReadU32(heap + 1408));
            lock_owned = 0;
            memory.WriteU32(frame_base + 84, 0);
        }

        memory.WriteU32(frame_base + 80, 0);
        if (services.FreeVirtualMemory(frame_base + 96, frame_base + 80,
                                       0x8000, 0) < 0)
            memory.WriteU32(frame_base + 88, 0);
    }
    else
    {
        memory.WriteU32(frame_base + 80, ReadU16(memory, block));
        block = CoalesceFreeBlocks(memory, services, heap, block,
                                   frame_base + 80, 0);
        const std::uint32_t units = memory.ReadU32(frame_base + 80);
        if (units < 128)
        {
            memory.WriteU8(block + 5, memory.ReadU8(block + 5) & 0x10);
            const GuestAddress list =
                SmallListHead(heap, memory.ReadU32(frame_base + 80) & 0xffff);
            if (memory.ReadU32(list) == list)
            {
                const std::uint32_t block_units = ReadU16(memory, block);
                const GuestAddress word = BitmapWord(heap, block_units);
                memory.WriteU32(word, memory.ReadU32(word) |
                                      (1u << (block_units & 31)));
            }

            const GuestAddress previous = memory.ReadU32(list + 4);
            const GuestAddress link = block + 8;
            memory.WriteU32(block + 8, list);
            memory.WriteU32(block + 12, previous);
            memory.WriteU32(previous, link);
            memory.WriteU32(list + 4, link);
            memory.WriteU32(heap + 48,
                            memory.ReadU32(heap + 48) + memory.ReadU32(frame_base + 80));
        }
        else
        {
            const std::uint32_t threshold = memory.ReadU32(heap + 40);
            if (units >= threshold &&
                memory.ReadU32(heap + 48) + units >= memory.ReadU32(heap + 44))
            {
                services.DecommitFreeBlock(heap, block, units);
            }
            else if (units > 0xf000)
            {
                InsertFreeBlocks(memory, heap, block, units);
            }
            else
            {
                const GuestAddress list = heap + 384;
                memory.WriteU8(block + 5, memory.ReadU8(block + 5) & 0x10);
                GuestAddress cursor = memory.ReadU32(list);
                memory.WriteU32(frame_base + 92, cursor);
                while (cursor != list)
                {
                    const std::uint32_t current_units = memory.ReadU32(frame_base + 80) & 0xffff;
                    if (current_units <= ReadU16(memory, cursor - 8))
                        break;
                    cursor = memory.ReadU32(cursor);
                    memory.WriteU32(frame_base + 92, cursor);
                }

                const GuestAddress previous = memory.ReadU32(cursor + 4);
                const GuestAddress link = block + 8;
                memory.WriteU32(block + 8, cursor);
                memory.WriteU32(block + 12, previous);
                memory.WriteU32(previous, link);
                memory.WriteU32(cursor + 4, link);
                memory.WriteU32(heap + 48,
                                memory.ReadU32(heap + 48) + memory.ReadU32(frame_base + 80));
            }
        }
    }

    LeaveHeapCriticalSection(memory, services, heap, lock_owned);
    return memory.ReadU32(frame_base + 88);
}

} // namespace lo::semantic::gpu
