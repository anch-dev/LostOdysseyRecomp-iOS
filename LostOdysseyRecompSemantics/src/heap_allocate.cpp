#include "lo_semantics/heap_allocate.h"

#include "lo_semantics/heap.h"
#include "lo_semantics/memory_fill.h"

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

GuestAddress BitmapWord(GuestAddress heap, std::uint32_t units)
{
    return heap + ((units >> 5) + 88) * 4;
}

GuestAddress SmallListHead(GuestAddress heap, std::uint32_t units)
{
    return heap + (units + 48) * 8;
}

// The original conditionally unlinks damaged links, but continues allocating.
// For a sole small-list entry it clears the bitmap bit using the block header.
void RemoveFreeLink(GuestMemory& memory, GuestAddress heap,
    GuestAddress block, bool clear_single_bitmap, bool small_only = false)
{
    const GuestAddress previous = memory.ReadU32(block + 12);
    const GuestAddress next = memory.ReadU32(block + 8);
    const GuestAddress observed_next = memory.ReadU32(previous);
    const GuestAddress observed_previous = memory.ReadU32(next + 4);
    if (observed_next != observed_previous || observed_next != block + 8)
        return;

    const bool sole_entry = next == previous;
    memory.WriteU32(previous, next);
    memory.WriteU32(next + 4, previous);
    if (sole_entry && clear_single_bitmap)
    {
        const std::uint32_t units = ReadU16(memory, block);
        if (!small_only || units < 128)
        {
            const GuestAddress word = BitmapWord(heap, units);
            memory.WriteU32(word, memory.ReadU32(word) ^ (1u << (units & 31)));
        }
    }
}

// This is the caller's inline <= F000 insertion, not 827CBA60. Its flag,
// bitmap, list, and free-unit stores have a different order from that helper.
void InsertRemainderInline(GuestMemory& memory, GuestAddress heap,
    GuestAddress block, std::uint32_t units, std::uint32_t units_to_add,
    GuestAddress frame_base, std::uint32_t cursor_slot)
{
    if (units < 128)
    {
        const GuestAddress head = SmallListHead(heap, units);
        memory.WriteU8(block + 5, memory.ReadU8(block + 5) & 0x10);
        if (memory.ReadU32(head) == head)
        {
            const std::uint32_t header_units = ReadU16(memory, block);
            const GuestAddress word = BitmapWord(heap, header_units);
            memory.WriteU32(word, memory.ReadU32(word) |
                                      (1u << (header_units & 31)));
        }
        const GuestAddress previous = memory.ReadU32(head + 4);
        const GuestAddress link = block + 8;
        memory.WriteU32(block + 8, head);
        memory.WriteU32(block + 12, previous);
        memory.WriteU32(previous, link);
        memory.WriteU32(head + 4, link);
    }
    else
    {
        memory.WriteU8(block + 5, memory.ReadU8(block + 5) & 0x10);
        const GuestAddress head = heap + 384;
        GuestAddress cursor = memory.ReadU32(head);
        memory.WriteU32(frame_base + cursor_slot, cursor);
        while (cursor != head)
        {
            if (units <= ReadU16(memory, cursor - 8))
                break;
            cursor = memory.ReadU32(cursor);
            memory.WriteU32(frame_base + cursor_slot, cursor);
        }
        const GuestAddress previous = memory.ReadU32(cursor + 4);
        const GuestAddress link = block + 8;
        memory.WriteU32(block + 8, cursor);
        memory.WriteU32(block + 12, previous);
        memory.WriteU32(previous, link);
        memory.WriteU32(cursor + 4, link);
    }
    memory.WriteU32(heap + 48, memory.ReadU32(heap + 48) + units_to_add);
}

void RecordLastRemainder(GuestMemory& memory, GuestAddress heap,
    GuestAddress remainder, GuestAddress frame_base)
{
    memory.WriteU8(frame_base + 80, 0);
    if ((memory.ReadU8(remainder + 5) & 0x10) != 0)
    {
        const std::uint32_t index = memory.ReadU8(remainder + 4);
        const GuestAddress segment = memory.ReadU32(heap + (index + 24) * 4);
        memory.WriteU32(segment + 64, remainder);
    }
}

} // namespace

void LeaveHeapAllocateCriticalSection(GuestMemory& memory,
    HeapAllocateServices& services, GuestAddress heap, std::uint32_t lock_owned)
{
    if (lock_owned != 0)
        services.LeaveCriticalSection(memory.ReadU32(heap + 1408));
}

GuestAddress AllocateHeapBlock(GuestMemory& memory, HeapAllocateServices& services,
    GuestAddress heap, std::uint32_t flags, std::uint32_t requested_bytes,
    GuestAddress frame_base)
{
    const std::uint32_t process_guard = memory.ReadU32(heap + 20);
    memory.WriteU32(frame_base + 128, heap);
    memory.WriteU32(frame_base + 100, 0);
    std::uint32_t lock_owned = 0;
    memory.WriteU32(frame_base + 104, 0);

    if ((process_guard & 0x40000) != 0)
    {
        const std::uint32_t process_type = services.GetCurrentProcessType();
        if (memory.ReadU8(heap + 379) != process_type)
            services.BugCheck(244, heap, memory.ReadU32(frame_base + 312),
                              1459, requested_bytes);
    }

    const std::uint32_t combined_flags = memory.ReadU32(heap + 24) | flags;
    const std::uint32_t normalized_request = requested_bytes ? requested_bytes : 1;
    const std::uint32_t rounded_bytes = (normalized_request + 31u) & 0xfffffff0u;
    memory.WriteU32(frame_base + 88, rounded_bytes);
    const std::uint32_t units = rounded_bytes >> 4;
    std::uint32_t rounded_argument = rounded_bytes;

    if ((combined_flags & 1) == 0)
    {
        services.EnterCriticalSection(memory.ReadU32(heap + 1408));
        lock_owned = 1;
        memory.WriteU32(frame_base + 104, lock_owned);
        rounded_argument = memory.ReadU32(frame_base + 88);
    }

    auto finish = [&]() -> GuestAddress {
        LeaveHeapAllocateCriticalSection(memory, services, heap, lock_owned);
        return memory.ReadU32(frame_base + 100);
    };
    auto fail = [&](std::uint32_t bytes) -> GuestAddress {
        if ((combined_flags & 4) != 0)
        {
            memory.WriteU32(frame_base + 144, 0xc0000017);
            memory.WriteU32(frame_base + 152, 0);
            memory.WriteU32(frame_base + 160, 1);
            memory.WriteU32(frame_base + 148, 0);
            memory.WriteU32(frame_base + 164, bytes);
            services.RaiseException(frame_base + 144);
        }
        memory.WriteU32(frame_base + 100, 0);
        return finish();
    };
    auto succeed = [&](GuestAddress allocated_block,
                       bool mark_last_block) -> GuestAddress {
        if (mark_last_block)
            memory.WriteU8(allocated_block + 5,
                           memory.ReadU8(allocated_block + 5) | 0x10);
        const GuestAddress payload = allocated_block + 16;
        memory.WriteU32(frame_base + 100, payload);
        if (lock_owned != 0)
        {
            services.LeaveCriticalSection(memory.ReadU32(heap + 1408));
            lock_owned = 0;
            memory.WriteU32(frame_base + 104, 0);
        }
        if ((combined_flags & 8) != 0)
            FillGuestMemory(memory, payload, 0, requested_bytes);
        return finish();
    };

    GuestAddress chosen_block = 0;
    bool from_bitmap = false;
    bool need_large_list = units >= 128;

    if (units < 128)
    {
        const GuestAddress head = SmallListHead(heap, units);
        if (memory.ReadU32(head) != head)
        {
            chosen_block = memory.ReadU32(head + 4) - 8;
            memory.WriteU32(frame_base + 92, chosen_block);
            const std::uint32_t old_flags = memory.ReadU8(chosen_block + 5);
            memory.WriteU8(frame_base + 80, static_cast<std::uint8_t>(old_flags));
            RemoveFreeLink(memory, heap, chosen_block, true);
            memory.WriteU32(heap + 48, memory.ReadU32(heap + 48) - units);
            memory.WriteU32(frame_base + 124, chosen_block);
            memory.WriteU8(chosen_block + 5, (old_flags & 0x10) | 1);
            const std::uint32_t unused_bytes = memory.ReadU32(frame_base + 88) - requested_bytes;
            memory.WriteU8(chosen_block + 6, static_cast<std::uint8_t>(unused_bytes));
            memory.WriteU8(chosen_block + 7, 0);
            return succeed(chosen_block, false);
        }

        const std::uint32_t group = units >> 5;
        GuestAddress bitmap_cursor = BitmapWord(heap, units);
        memory.WriteU32(frame_base + 96, bitmap_cursor);
        const std::uint32_t bitmap = memory.ReadU32(bitmap_cursor);
        std::uint32_t candidate = bitmap & ~((1u << (units & 31)) - 1u);
        memory.WriteU32(frame_base + 108, candidate);
        bitmap_cursor += 4;
        memory.WriteU32(frame_base + 96, bitmap_cursor);

        for (std::uint32_t candidate_group = group; candidate_group < 4; ++candidate_group)
        {
            if (candidate != 0)
            {
                const std::uint32_t bit = std::countr_zero(candidate);
                const GuestAddress candidate_head = heap + 384 + candidate_group * 256 + bit * 8;
                chosen_block = memory.ReadU32(candidate_head + 4) - 8;
                memory.WriteU32(frame_base + 92, chosen_block);
                from_bitmap = true;
                break;
            }
            if (candidate_group == 3)
                break;
            candidate = memory.ReadU32(bitmap_cursor);
            memory.WriteU32(frame_base + 108, candidate);
            bitmap_cursor += 4;
            memory.WriteU32(frame_base + 96, bitmap_cursor);
        }
        need_large_list = !from_bitmap;
    }

    if (need_large_list)
    {
        if (units >= 128 && units > memory.ReadU32(heap + 28))
        {
            if ((memory.ReadU32(heap + 20) & 2) == 0)
                return fail(rounded_argument);

            memory.WriteU32(frame_base + 84, 0);
            memory.WriteU32(frame_base + 88, rounded_argument + 32);
            const std::uint32_t type = (combined_flags & 8) != 0 ?
                0x60001000u : 0x60801000u;
            if (services.AllocateVirtualMemory(frame_base + 84, frame_base + 88,
                                               type, 4, 0) < 0)
                return fail(memory.ReadU32(frame_base + 88));

            FillGuestMemory(memory, memory.ReadU32(frame_base + 84), 0, 48);
            const GuestAddress list = heap + 88;
            const std::uint32_t unused = memory.ReadU32(frame_base + 88) - requested_bytes;
            const GuestAddress vm_base = memory.ReadU32(frame_base + 84);
            WriteU16(memory, vm_base + 32, unused + 65536 - 48);
            memory.WriteU8(memory.ReadU32(frame_base + 84) + 37, 11);
            memory.WriteU32(memory.ReadU32(frame_base + 84) + 24,
                            memory.ReadU32(frame_base + 88));
            memory.WriteU32(memory.ReadU32(frame_base + 84) + 28,
                            memory.ReadU32(frame_base + 88));
            const GuestAddress previous = memory.ReadU32(list + 4);
            memory.WriteU32(memory.ReadU32(frame_base + 84), list);
            memory.WriteU32(memory.ReadU32(frame_base + 84) + 4, previous);
            memory.WriteU32(previous, memory.ReadU32(frame_base + 84));
            const GuestAddress final_vm_base = memory.ReadU32(frame_base + 84);
            memory.WriteU32(list + 4, final_vm_base);
            memory.WriteU32(frame_base + 100, final_vm_base + 48);
            return finish();
        }

        const GuestAddress head = heap + 384;
        const GuestAddress tail = memory.ReadU32(head + 4);
        memory.WriteU32(frame_base + 112, tail);
        bool grow = tail == head;
        if (!grow)
        {
            memory.WriteU32(frame_base + 92, tail - 8);
            grow = ReadU16(memory, tail - 8) < units;
        }
        if (grow)
        {
            chosen_block = services.GrowHeap(heap, rounded_argument);
            memory.WriteU32(frame_base + 92, chosen_block);
            if (chosen_block == 0)
                return fail(memory.ReadU32(frame_base + 88));
        }
        else
        {
            GuestAddress cursor = memory.ReadU32(head);
            memory.WriteU32(frame_base + 112, cursor);
            while (cursor != head)
            {
                chosen_block = cursor - 8;
                memory.WriteU32(frame_base + 92, chosen_block);
                if (ReadU16(memory, chosen_block) >= units)
                    break;
                cursor = memory.ReadU32(cursor);
                memory.WriteU32(frame_base + 112, cursor);
            }
            if (cursor == head)
            {
                chosen_block = services.GrowHeap(heap, rounded_argument);
                memory.WriteU32(frame_base + 92, chosen_block);
                if (chosen_block == 0)
                    return fail(memory.ReadU32(frame_base + 88));
            }
        }
    }

    RemoveFreeLink(memory, heap, chosen_block, from_bitmap);
    const std::uint32_t requested_units_low = units & 0xffff;
    const std::uint32_t old_flags = memory.ReadU8(chosen_block + 5);
    memory.WriteU8(frame_base + 80, static_cast<std::uint8_t>(old_flags));
    const std::uint32_t original_units = ReadU16(memory, chosen_block);
    memory.WriteU32(heap + 48, memory.ReadU32(heap + 48) - original_units);
    memory.WriteU32(frame_base + 124, chosen_block);
    memory.WriteU8(chosen_block + 5, 1);
    const std::uint32_t remainder_units = ReadU16(memory, chosen_block) - units;
    WriteU16(memory, chosen_block, requested_units_low);
    memory.WriteU8(chosen_block + 6,
                   static_cast<std::uint8_t>(memory.ReadU32(frame_base + 88) - requested_bytes));
    memory.WriteU8(chosen_block + 7, 0);

    if (remainder_units == 0)
        return succeed(chosen_block, (old_flags & 0x10) != 0);
    if (remainder_units == 1)
    {
        WriteU16(memory, chosen_block, ReadU16(memory, chosen_block) + 1);
        memory.WriteU8(chosen_block + 6, memory.ReadU8(chosen_block + 6) + 16);
        return succeed(chosen_block, (old_flags & 0x10) != 0);
    }

    const GuestAddress remainder = chosen_block + units * 16;
    memory.WriteU8(remainder + 5, static_cast<std::uint8_t>(old_flags));
    WriteU16(memory, remainder + 2, requested_units_low);
    memory.WriteU8(remainder + 4, memory.ReadU8(chosen_block + 4));
    const std::uint32_t remainder_low = remainder_units & 0xffff;
    WriteU16(memory, remainder, remainder_low);

    if ((old_flags & 0x10) != 0)
    {
        InsertRemainderInline(memory, heap, remainder, remainder_low,
                              remainder_units, frame_base, 116);
    }
    else
    {
        const GuestAddress next_block = remainder + remainder_units * 16;
        const std::uint32_t next_flags = memory.ReadU8(next_block + 5);
        if ((next_flags & 1) != 0)
        {
            WriteU16(memory, next_block + 2, remainder_low);
            InsertRemainderInline(memory, heap, remainder, remainder_low,
                                  remainder_units, frame_base, 132);
        }
        else
        {
            memory.WriteU8(remainder + 5, static_cast<std::uint8_t>(next_flags));
            RemoveFreeLink(memory, heap, next_block, true, true);
            memory.WriteU32(heap + 48,
                            memory.ReadU32(heap + 48) - ReadU16(memory, next_block));
            const std::uint32_t merged_units = ReadU16(memory, next_block) + remainder_units;
            if (merged_units > 0xf000)
            {
                InsertFreeBlocks(memory, heap, remainder, merged_units);
            }
            else
            {
                const std::uint32_t merged_low = merged_units & 0xffff;
                WriteU16(memory, remainder, merged_low);
                if ((memory.ReadU8(remainder + 5) & 0x10) == 0)
                    WriteU16(memory, remainder + merged_units * 16 + 2, merged_low);
                InsertRemainderInline(memory, heap, remainder, merged_low,
                                      merged_units, frame_base, 120);
            }
        }
    }

    RecordLastRemainder(memory, heap, remainder, frame_base);
    return succeed(chosen_block, false);
}

} // namespace lo::semantic::gpu
