#include "lo_semantics/heap_decommit.h"

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

void WriteU16(GuestMemory& memory, GuestAddress address, std::uint32_t value)
{
    memory.WriteU8(address, static_cast<std::uint8_t>(value >> 8));
    memory.WriteU8(address + 1, static_cast<std::uint8_t>(value));
}

void InsertIntoFreeList(GuestMemory& memory, GuestAddress heap,
    GuestAddress block, std::uint32_t units)
{
    GuestAddress cursor;
    if (units < 128)
    {
        cursor = heap + (units + 48) * 8;
        if (memory.ReadU32(cursor) == cursor)
        {
            const std::uint32_t size = ReadU16(memory, block);
            const GuestAddress word = heap + ((size >> 5) + 88) * 4;
            memory.WriteU32(word, memory.ReadU32(word) | (1u << (size & 31)));
        }
    }
    else
    {
        const GuestAddress head = heap + 384;
        cursor = memory.ReadU32(head);
        while (cursor != head)
        {
            if (units <= ReadU16(memory, cursor - 8))
                break;
            cursor = memory.ReadU32(cursor);
        }
    }

    const GuestAddress previous = memory.ReadU32(cursor + 4);
    const GuestAddress link = block + 8;
    memory.WriteU32(block + 8, cursor);
    memory.WriteU32(block + 12, previous);
    memory.WriteU32(previous, link);
    memory.WriteU32(cursor + 4, link);
}

} // namespace

void DecommitFreeBlock(GuestMemory& memory, HeapDecommitServices& services,
    GuestAddress heap, GuestAddress block, std::uint32_t units,
    GuestAddress frame_base)
{
    auto insert_whole_block = [&]() {
        InsertFreeBlocks(memory, heap, block, units);
    };

    if (memory.ReadU32(heap + 1412) != 0)
    {
        insert_whole_block();
        return;
    }

    const std::uint32_t segment_index = memory.ReadU8(block + 4);
    const GuestAddress segment = memory.ReadU32(heap + (segment_index + 24) * 4);
    GuestAddress range_base = (block + 0xffffu) & 0xffff0000u;
    memory.WriteU32(frame_base + 84, range_base);

    std::uint32_t leading_units = ((range_base - block) >> 4) & 0xffffu;
    GuestAddress preceding_block = 0;
    if (leading_units == 1)
    {
        range_base += 0x10000u;
        leading_units = 4097;
        memory.WriteU32(frame_base + 84, range_base);
    }
    else if (ReadU16(memory, block + 2) != 0 && range_base == block)
    {
        preceding_block = block - (std::uint32_t{ReadU16(memory, block + 2)} << 4);
    }

    const GuestAddress end = block + (units << 4);
    GuestAddress range_end = end & 0xffff0000u;
    std::uint32_t trailing_units = ((end - range_end) >> 4) & 0xffffu;
    GuestAddress following_block = 0;
    if (trailing_units == 1)
    {
        trailing_units = 4097;
        range_end -= 0x10000u;
    }
    else if (trailing_units == 0 && (memory.ReadU8(block + 5) & 0x10) == 0)
    {
        following_block = end;
    }

    const std::uint32_t trailing_bytes = (trailing_units & 0xffffu) << 4;
    const GuestAddress trailing_block = end - trailing_bytes;
    const std::uint32_t range_bytes = range_end > range_base ?
        range_end - range_base : 0;
    memory.WriteU32(frame_base + 80, range_bytes);
    if (range_bytes == 0)
    {
        insert_whole_block();
        return;
    }

    const GuestAddress node = services.AllocateRangeNode(segment);
    if (node == 0)
    {
        insert_whole_block();
        return;
    }

    const std::int32_t status = services.FreeVirtualMemory(
        frame_base + 84, frame_base + 80, 0x4000, 0);
    const GuestAddress range_header = memory.ReadU32(segment + 24);
    memory.WriteU32(node, memory.ReadU32(range_header + 76));
    memory.WriteU32(memory.ReadU32(segment + 24) + 76, node);
    memory.WriteU32(node + 4, 0);
    memory.WriteU32(node + 8, 0);
    if (status < 0)
    {
        insert_whole_block();
        return;
    }

    services.InsertRangeRecord(segment, memory.ReadU32(frame_base + 84),
                               memory.ReadU32(frame_base + 80));
    const std::uint32_t released_ranges = (memory.ReadU32(frame_base + 80) >> 16) & 0xffffu;
    memory.WriteU32(segment + 48, memory.ReadU32(segment + 48) + released_ranges);

    if (leading_units != 0)
    {
        WriteU16(memory, block, leading_units);
        memory.WriteU8(block + 5, 0x10);
        memory.WriteU32(heap + 48, memory.ReadU32(heap + 48) + leading_units);
        memory.WriteU32(segment + 64, block);
        memory.WriteU8(block + 5, memory.ReadU8(block + 5) & ~7u);
        InsertIntoFreeList(memory, heap, block, leading_units);
    }
    else if (preceding_block != 0)
    {
        memory.WriteU8(preceding_block + 5,
                       memory.ReadU8(preceding_block + 5) | 0x10);
        memory.WriteU32(segment + 64, preceding_block);
    }
    else
    {
        const GuestAddress last_block = memory.ReadU32(segment + 64);
        const GuestAddress released_base = memory.ReadU32(frame_base + 84);
        if (last_block >= released_base &&
            last_block < released_base + memory.ReadU32(frame_base + 80))
            memory.WriteU32(segment + 64, memory.ReadU32(segment + 40));
    }

    if (trailing_units != 0)
    {
        WriteU16(memory, trailing_block + 2, 0);
        const std::uint32_t new_segment_index = memory.ReadU8(segment + 4);
        memory.WriteU8(trailing_block + 5, 0);
        WriteU16(memory, trailing_block, trailing_units);
        memory.WriteU8(trailing_block + 4,
                       static_cast<std::uint8_t>(new_segment_index));
        WriteU16(memory, end + 2, trailing_units);
        memory.WriteU8(trailing_block + 5,
                       memory.ReadU8(trailing_block + 5) & ~7u);
        InsertIntoFreeList(memory, heap, trailing_block, trailing_units);
        memory.WriteU32(heap + 48, memory.ReadU32(heap + 48) + trailing_units);
    }
    else if (following_block != 0)
    {
        WriteU16(memory, following_block + 2, 0);
    }
}

} // namespace lo::semantic::gpu
