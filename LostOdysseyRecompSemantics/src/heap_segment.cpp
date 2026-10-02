#include "lo_semantics/heap_segment.h"

#include "lo_semantics/heap.h"

#include <bit>

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

std::uint32_t TruncateSignedPages(std::uint32_t difference)
{
    // srawi 16 followed by addze rounds negative fractional pages toward zero.
    return static_cast<std::uint32_t>(std::bit_cast<std::int32_t>(difference) / 65536);
}

std::uint32_t ArithmeticShiftRightFour(std::uint32_t difference)
{
    const std::int64_t signed_value = std::bit_cast<std::int32_t>(difference);
    const std::int64_t shifted = signed_value >= 0 ? signed_value / 16 :
        -((-signed_value + 15) / 16);
    return static_cast<std::uint32_t>(shifted);
}

} // namespace

GuestAddress ExtendHeapSegment(GuestMemory& memory, HeapSegmentServices& services,
    GuestAddress heap, GuestAddress segment, GuestAddress bytes_inout,
    GuestAddress required_base, GuestAddress frame_base)
{
    const GuestAddress range_head = segment + 56;
    GuestAddress previous_link = range_head;
    GuestAddress previous_range = 0;
    GuestAddress range = memory.ReadU32(range_head);
    if (range == 0)
        return 0;
    const std::uint32_t requested = memory.ReadU32(bytes_inout);

    while (range != 0)
    {
        const std::uint32_t available = memory.ReadU32(range + 8);
        if (available >= requested &&
            (required_base == 0 || memory.ReadU32(range + 4) == required_base))
            break;
        previous_range = range;
        previous_link = range;
        range = memory.ReadU32(range);
    }
    if (range == 0)
        return 0;

    const GuestAddress range_base = memory.ReadU32(range + 4);
    const GuestAddress commit_callback = memory.ReadU32(heap + 1412);
    memory.WriteU32(frame_base + 80, range_base);
    const std::int32_t status = commit_callback != 0 ?
        services.CommitRange(commit_callback & ~GuestAddress{3}, heap,
                             frame_base + 80, bytes_inout) :
        services.AllocateVirtualMemory(frame_base + 80, bytes_inout,
                                       0x60001000, 4, 0);
    if (status < 0)
        return 0;

    const std::uint32_t free_pages = memory.ReadU32(segment + 48);
    const std::uint32_t committed_units = ReadU16(memory, bytes_inout);
    const std::uint32_t largest_range = memory.ReadU32(segment + 28);
    memory.WriteU32(segment + 48, free_pages - committed_units);
    if (largest_range == memory.ReadU32(range + 8))
        memory.WriteU32(segment + 28, 0);

    GuestAddress new_block = memory.ReadU32(frame_base + 80);
    GuestAddress preceding_block = memory.ReadU32(segment + 64);
    bool found_boundary = false;
    if ((memory.ReadU8(preceding_block + 5) & 0x10) != 0)
    {
        const std::uint32_t preceding_units = ReadU16(memory, preceding_block);
        if (preceding_block + std::rotl(preceding_units, 4) ==
            memory.ReadU32(range + 4))
            found_boundary = true;
    }

    if (!found_boundary)
    {
        if (previous_range == 0)
            preceding_block = memory.ReadU32(segment + 40);
        else
        {
            const std::uint32_t size = memory.ReadU32(previous_range + 8);
            const GuestAddress start = memory.ReadU32(previous_range + 4);
            preceding_block = start + size;
        }

        if ((memory.ReadU8(preceding_block + 5) & 0x10) == 0)
        {
            const GuestAddress segment_end = memory.ReadU32(segment + 44);
            for (;;)
            {
                const std::uint32_t units = ReadU16(memory, preceding_block);
                const GuestAddress current = preceding_block;
                preceding_block += std::rotl(units, 4);
                if (preceding_block >= segment_end ||
                    ReadU16(memory, preceding_block) == 0)
                {
                    if (preceding_block != new_block)
                        return 0;
                    preceding_block = current;
                    break;
                }
                if ((memory.ReadU8(preceding_block + 5) & 0x10) != 0)
                    break;
            }
        }
    }

    memory.WriteU8(preceding_block + 5, memory.ReadU8(preceding_block + 5) & ~0x10u);
    const std::uint32_t committed_bytes = memory.ReadU32(bytes_inout);
    const GuestAddress current_base = memory.ReadU32(range + 4);
    const std::uint32_t current_size = memory.ReadU32(range + 8);
    memory.WriteU32(range + 4, current_base + committed_bytes);
    const std::uint32_t remaining_bytes = current_size - memory.ReadU32(bytes_inout);
    memory.WriteU32(range + 8, remaining_bytes);

    if (remaining_bytes == 0)
    {
        const GuestAddress range_end = memory.ReadU32(range + 4);
        if (range_end == memory.ReadU32(segment + 44))
        {
            memory.WriteU8(new_block + 5, 16);
            memory.WriteU32(segment + 64, new_block);
        }
        else
        {
            memory.WriteU8(new_block + 5, 0);
            memory.WriteU32(segment + 64, memory.ReadU32(segment + 40));
        }

        memory.WriteU32(previous_link, memory.ReadU32(range));
        const GuestAddress owner = memory.ReadU32(segment + 24);
        memory.WriteU32(range, memory.ReadU32(owner + 76));
        memory.WriteU32(memory.ReadU32(segment + 24) + 76, range);
        memory.WriteU32(range + 4, 0);
        memory.WriteU32(range + 8, 0);
        memory.WriteU32(segment + 52, memory.ReadU32(segment + 52) - 1);
    }
    else
    {
        memory.WriteU8(new_block + 5, 16);
        memory.WriteU32(segment + 64, new_block);
    }

    const std::uint32_t preceding_index = memory.ReadU8(preceding_block + 4);
    const std::uint32_t new_flags = memory.ReadU8(new_block + 5);
    memory.WriteU8(new_block + 4, static_cast<std::uint8_t>(preceding_index));
    const std::uint32_t new_units = memory.ReadU32(bytes_inout) >> 4;
    WriteU16(memory, new_block, new_units);
    const std::uint32_t preceding_size = ReadU16(memory, preceding_block);
    WriteU16(memory, new_block + 2, preceding_size);
    if ((new_flags & 0x10) == 0)
        WriteU16(memory, new_block + std::rotl(new_units & 0xffffu, 4) + 2,
                 new_units & 0xffffu);

    if (memory.ReadU32(segment + 28) == 0)
    {
        GuestAddress current = memory.ReadU32(range_head);
        while (current != 0)
        {
            const std::uint32_t size = memory.ReadU32(current + 8);
            if (size >= memory.ReadU32(segment + 28))
                memory.WriteU32(segment + 28, size);
            current = memory.ReadU32(current);
        }
    }
    return new_block;
}

GuestAddress InitializeHeapSegment(GuestMemory& memory,
    HeapSegmentServices& services, GuestAddress heap, GuestAddress base,
    std::uint32_t slot_index, std::uint32_t segment_flags,
    GuestAddress range_begin, GuestAddress committed_end,
    GuestAddress reserved_end, GuestAddress frame_base)
{
    memory.WriteU32(frame_base + 236, committed_end);
    const std::uint32_t reserved_pages = TruncateSignedPages(reserved_end - range_begin);
    const GuestAddress first_block = (base + 87u) & 0xfffffff0u;
    const std::uint32_t previous_units = heap == range_begin ? ReadU16(memory, heap) : 0;
    const std::uint32_t prefix_units = ArithmeticShiftRightFour(first_block - base) & 0xffff;

    if (first_block + 16 >= committed_end)
    {
        if (first_block + 16 >= reserved_end)
            return 0;
        memory.WriteU32(frame_base + 80, first_block - committed_end + 16);
        if (services.AllocateVirtualMemory(frame_base + 236, frame_base + 80,
                                           0x60001000, 4, 0) < 0)
            return 0;
        const GuestAddress new_commit_base = memory.ReadU32(frame_base + 236);
        const std::uint32_t new_commit_bytes = memory.ReadU32(frame_base + 80);
        committed_end = new_commit_base + new_commit_bytes;
        memory.WriteU32(frame_base + 236, committed_end);
    }

    const std::uint32_t remaining_pages = TruncateSignedPages(reserved_end - committed_end);
    WriteU16(memory, base + 2, previous_units);
    WriteU16(memory, base, prefix_units);
    memory.WriteU8(base + 4, static_cast<std::uint8_t>(slot_index));
    memory.WriteU32(base + 20, segment_flags);
    memory.WriteU32(base + 24, heap);
    memory.WriteU32(base + 32, range_begin);
    memory.WriteU32(base + 40, first_block);
    memory.WriteU32(base + 36, reserved_pages);
    memory.WriteU32(base + 48, remaining_pages);
    memory.WriteU8(base + 5, 1);
    memory.WriteU32(base + 16, 0xffeeffee);
    memory.WriteU32(base + 44, range_begin + (reserved_pages << 16));

    if (remaining_pages != 0)
    {
        services.InitializeUncommittedRange(base, committed_end,
                                             remaining_pages << 16);
        committed_end = memory.ReadU32(frame_base + 236);
    }

    const std::uint32_t free_units = ArithmeticShiftRightFour(committed_end - first_block);
    memory.WriteU32(heap + ((slot_index & 0xffu) + 24) * 4, base);
    const std::uint32_t header_units = ReadU16(memory, base);
    memory.WriteU8(first_block + 5, 16);
    memory.WriteU32(base + 64, first_block);
    memory.WriteU8(first_block + 4, static_cast<std::uint8_t>(slot_index));
    WriteU16(memory, first_block + 2, header_units);
    InsertFreeBlocks(memory, heap, first_block, free_units);
    return 1;
}

} // namespace lo::semantic::gpu
