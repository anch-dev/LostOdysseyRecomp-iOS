#include "lo_semantics/heap.h"

namespace lo::semantic::gpu
{
namespace
{

std::uint16_t ReadU16(GuestMemory& memory, GuestAddress address)
{
    return static_cast<std::uint16_t>((std::uint16_t{memory.ReadU8(address)} << 8) |
                                      memory.ReadU8(address + 1));
}

void WriteU16(GuestMemory& memory, GuestAddress address, std::uint16_t value)
{
    memory.WriteU8(address, static_cast<std::uint8_t>(value >> 8));
    memory.WriteU8(address + 1, static_cast<std::uint8_t>(value));
}

GuestAddress SegmentHead(GuestMemory& memory, GuestAddress heap, GuestAddress block)
{
    return memory.ReadU32(heap + (std::uint32_t{memory.ReadU8(block + 4)} + 24) * 4);
}

GuestAddress SmallListHead(GuestAddress heap, std::uint32_t units)
{
    return heap + (units + 48) * 8;
}

GuestAddress BitmapWord(GuestAddress heap, std::uint32_t units)
{
    return heap + ((units >> 5) + 88) * 4;
}

void MaybeCompareFreeFill(GuestMemory& memory, HeapServices& services,
    GuestAddress block)
{
    const std::uint8_t flags = memory.ReadU8(block + 5);
    if ((flags & 4) == 0)
        return;

    std::uint32_t bytes = (std::uint32_t{ReadU16(memory, block)} << 4) - 24;
    if ((flags & 2) != 0 && bytes > 4)
        bytes -= 4;
    (void)services.CompareMemoryUlong(block + 24, bytes, 0xfeeefeee);
}

// An inconsistent backlink skips the two unlink stores. The original still
// proceeds with coalescing, fill checks and free-unit accounting afterward.
void TryUnlink(GuestMemory& memory, GuestAddress heap, GuestAddress block)
{
    const GuestAddress tail = memory.ReadU32(block + 12);
    const GuestAddress link = block + 8;
    const GuestAddress head = memory.ReadU32(block + 8);
    const GuestAddress tail_next = memory.ReadU32(tail);
    const GuestAddress head_prev = memory.ReadU32(head + 4);
    if (tail_next != head_prev || tail_next != link)
        return;

    memory.WriteU32(tail, head);
    memory.WriteU32(head + 4, tail);
    if (head == tail)
    {
        const std::uint32_t units = ReadU16(memory, block);
        if (units < 128)
        {
            const GuestAddress word = BitmapWord(heap, units);
            memory.WriteU32(word, memory.ReadU32(word) ^ (1u << (units & 31)));
        }
    }
}

void SubtractFreeUnits(GuestMemory& memory, GuestAddress heap, GuestAddress block)
{
    const std::uint32_t units = ReadU16(memory, block);
    const std::uint32_t free_units = memory.ReadU32(heap + 48);
    memory.WriteU32(heap + 48, free_units - units);
}

void SubtractFreeUnitsHeapFirst(GuestMemory& memory, GuestAddress heap,
    GuestAddress block)
{
    const std::uint32_t free_units = memory.ReadU32(heap + 48);
    const std::uint32_t units = ReadU16(memory, block);
    memory.WriteU32(heap + 48, free_units - units);
}

void SetLastBlock(GuestMemory& memory, GuestAddress heap, GuestAddress block)
{
    const GuestAddress segment = SegmentHead(memory, heap, block);
    memory.WriteU32(segment + 64, block);
}

} // namespace

void InsertFreeBlocks(GuestMemory& memory, GuestAddress heap, GuestAddress block,
    std::uint32_t units)
{
    const std::uint32_t free_units = memory.ReadU32(heap + 48);
    const std::uint8_t segment_index = memory.ReadU8(block + 4);
    const std::uint32_t total = free_units + units;
    std::uint16_t previous_units = ReadU16(memory, block + 2);
    const std::uint8_t original_flags = memory.ReadU8(block + 5);
    const GuestAddress segment = memory.ReadU32(heap + (std::uint32_t{segment_index} + 24) * 4);
    memory.WriteU32(heap + 48, total);

    for (;;)
    {
        if (units == 0)
        {
            if ((original_flags & 0x10) == 0)
                WriteU16(memory, block + 2, previous_units);
            return;
        }

        const std::uint32_t chunk = units > 0xf000 ?
            (units == 0xf001 ? 0xeff0u : 0xf000u) : units & 0xffffu;
        if (units > 0xf000)
            memory.WriteU8(block + 5, 0);
        else
            memory.WriteU8(block + 5, original_flags);

        const std::uint8_t flags = memory.ReadU8(block + 5);
        WriteU16(memory, block + 2, previous_units);
        memory.WriteU8(block + 4, segment_index);
        WriteU16(memory, block, static_cast<std::uint16_t>(chunk));
        memory.WriteU8(block + 5, static_cast<std::uint8_t>(flags & ~7u));

        GuestAddress cursor;
        if (chunk < 128)
        {
            cursor = SmallListHead(heap, chunk);
            if (memory.ReadU32(cursor) == cursor)
            {
                const GuestAddress word = BitmapWord(heap, chunk);
                memory.WriteU32(word, memory.ReadU32(word) | (1u << (chunk & 31)));
            }
        }
        else
        {
            const GuestAddress large_head = heap + 384;
            cursor = memory.ReadU32(large_head);
            while (cursor != large_head)
            {
                if (chunk <= ReadU16(memory, cursor - 8))
                    break;
                cursor = memory.ReadU32(cursor);
            }
        }

        const GuestAddress next = memory.ReadU32(cursor + 4);
        const GuestAddress link = block + 8;
        memory.WriteU32(block + 8, cursor);
        units -= chunk;
        memory.WriteU32(block + 12, next);
        block += chunk << 4;
        memory.WriteU32(next, link);
        memory.WriteU32(cursor + 4, link);
        previous_units = static_cast<std::uint16_t>(chunk);

        if (block >= memory.ReadU32(segment + 44))
            return;
    }
}

GuestAddress CoalesceFreeBlocks(GuestMemory& memory, HeapServices& services,
    GuestAddress heap, GuestAddress block, GuestAddress units_inout,
    std::uint32_t already_free)
{
    const std::uint32_t previous_distance = std::uint32_t{ReadU16(memory, block + 2)} << 4;
    GuestAddress previous = block - previous_distance;
    if (previous != block && (memory.ReadU8(previous + 5) & 1) == 0 &&
        std::uint32_t{ReadU16(memory, previous)} + memory.ReadU32(units_inout) <= 0xf000)
    {
        if (already_free != 0)
        {
            TryUnlink(memory, heap, block);
            MaybeCompareFreeFill(memory, services, block);
            already_free = 0;
            SubtractFreeUnits(memory, heap, block);
        }

        TryUnlink(memory, heap, previous);
        MaybeCompareFreeFill(memory, services, previous);
        const std::uint8_t last_flag = memory.ReadU8(block + 5) & 0x10;
        memory.WriteU8(previous + 5, last_flag);
        if (last_flag != 0)
            SetLastBlock(memory, heap, previous);

        const std::uint32_t combined = memory.ReadU32(units_inout) + ReadU16(memory, previous);
        memory.WriteU32(units_inout, combined);
        SubtractFreeUnitsHeapFirst(memory, heap, previous);
        const std::uint8_t merged_flags = memory.ReadU8(previous + 5);
        const std::uint32_t merged_units = memory.ReadU32(units_inout);
        WriteU16(memory, previous, static_cast<std::uint16_t>(merged_units));
        if ((merged_flags & 0x10) == 0)
        {
            const std::uint32_t trailing_units = memory.ReadU32(units_inout);
            WriteU16(memory, previous + (trailing_units << 4) + 2,
                     static_cast<std::uint16_t>(trailing_units));
        }
        block = previous;
    }

    if ((memory.ReadU8(block + 5) & 0x10) == 0)
    {
        const GuestAddress next = block + (memory.ReadU32(units_inout) << 4);
        if ((memory.ReadU8(next + 5) & 1) == 0 &&
            std::uint32_t{ReadU16(memory, next)} + memory.ReadU32(units_inout) <= 0xf000)
        {
            if (already_free != 0)
            {
                TryUnlink(memory, heap, block);
                MaybeCompareFreeFill(memory, services, block);
                SubtractFreeUnits(memory, heap, block);
            }

            const std::uint8_t next_last_flag = memory.ReadU8(next + 5) & 0x10;
            memory.WriteU8(block + 5, next_last_flag);
            if (next_last_flag != 0)
                SetLastBlock(memory, heap, block);

            TryUnlink(memory, heap, next);
            MaybeCompareFreeFill(memory, services, next);
            const std::uint32_t combined = memory.ReadU32(units_inout) + ReadU16(memory, next);
            memory.WriteU32(units_inout, combined);
            SubtractFreeUnitsHeapFirst(memory, heap, next);
            const std::uint8_t merged_flags = memory.ReadU8(block + 5);
            const std::uint32_t merged_units = memory.ReadU32(units_inout);
            WriteU16(memory, block, static_cast<std::uint16_t>(merged_units));
            if ((merged_flags & 0x10) == 0)
            {
                const std::uint32_t trailing_units = memory.ReadU32(units_inout);
                WriteU16(memory, block + (trailing_units << 4) + 2,
                         static_cast<std::uint16_t>(trailing_units));
            }
        }
    }
    return block;
}

} // namespace lo::semantic::gpu
