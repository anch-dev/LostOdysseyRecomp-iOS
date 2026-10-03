#include "lo_semantics/heap_block_resize.h"

#include "lo_semantics/memory_fill.h"

#include <bit>

namespace lo::semantic::gpu::heap_block_resize
{
namespace
{
constexpr GuestAddress kAddress = 0x827CBCA8u;
constexpr std::uint32_t kFreeFill = 0xFEEEFEEEu;

std::uint64_t& R(FrameRegisters& frame, unsigned number)
{ return frame.r22_through_r31[number - 22u]; }

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}

void Enter(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<GuestAddress>(frame.lr));
    for (unsigned reg = 22; reg <= 31; ++reg)
        WriteU64(memory, sp - 8u * (33u - reg), R(frame, reg));
    memory.WriteU32(sp - 176u, sp);
    frame.sp = caller_sp - 176u;
}

void Leave(GuestMemory& memory, FrameRegisters& frame)
{
    frame.sp += 176u;
    const GuestAddress sp = static_cast<GuestAddress>(frame.sp);
    frame.lr = memory.ReadU32(sp - 8u);
    for (unsigned reg = 22; reg <= 31; ++reg)
        R(frame, reg) = ReadU64(memory, sp - 8u * (33u - reg));
}

GuestAddress Slot(FrameRegisters& frame)
{ return static_cast<GuestAddress>(frame.sp + 80u); }

GuestAddress Segment(GuestMemory& memory, GuestAddress heap,
    GuestAddress block)
{
    return memory.ReadU32(heap + (std::uint32_t{memory.ReadU8(block + 4u)} + 24u) * 4u);
}

void SetLast(GuestMemory& memory, GuestAddress heap, GuestAddress block,
    GuestAddress last)
{ memory.WriteU32(Segment(memory, heap, block) + 64u, last); }

GuestAddress BitmapWord(GuestAddress heap, std::uint32_t units)
{ return heap + ((units >> 5u) + 88u) * 4u; }

void UnlinkFreeBlock(GuestMemory& memory, GuestAddress heap,
    GuestAddress block, FrameRegisters& frame)
{
    const GuestAddress tail = memory.ReadU32(block + 12u);
    const GuestAddress head = memory.ReadU32(block + 8u);
    const GuestAddress backlink = memory.ReadU32(tail);
    if (backlink != memory.ReadU32(head + 4u) || backlink != block + 8u)
        return;
    memory.WriteU32(tail, head);
    memory.WriteU32(head + 4u, tail);
    if (head == tail)
    {
        const auto units = memory.ReadU16(block);
        if (units < 128u)
        {
            const auto word = BitmapWord(heap, units);
            memory.WriteU32(word, memory.ReadU32(word) ^
                (static_cast<std::uint32_t>(R(frame, 25)) << (units & 31u)));
        }
    }
}

void CompareFreeFill(GuestMemory& memory, NativeServices& services,
    FrameRegisters& frame, GuestAddress block, std::uint64_t return_lr)
{
    const auto flags = memory.ReadU8(block + 5u);
    if ((flags & 4u) == 0)
        return;
    std::uint32_t bytes = (std::uint32_t{memory.ReadU16(block)} << 4u) - 24u;
    if ((flags & 2u) != 0 && bytes > 4u)
        bytes -= 4u;
    frame.lr = return_lr;
    (void)services.CompareMemoryUlong(memory, block + 24u, bytes,
        kFreeFill, frame.sp, frame);
}

GuestAddress InsertionCursor(GuestMemory& memory, GuestAddress heap,
    std::uint32_t units, FrameRegisters& frame)
{
    if (units < 128u)
    {
        const GuestAddress head = heap + (units + 48u) * 8u;
        if (memory.ReadU32(head) == head)
        {
            const GuestAddress word = BitmapWord(heap, units);
            memory.WriteU32(word,
                memory.ReadU32(word) |
                (static_cast<std::uint32_t>(R(frame, 25)) << (units & 31u)));
        }
        return head;
    }
    const GuestAddress head = heap + 384u;
    GuestAddress cursor = memory.ReadU32(head);
    while (cursor != head)
    {
        if (units <= memory.ReadU16(cursor - 8u))
            break;
        cursor = memory.ReadU32(cursor);
    }
    return cursor;
}

void InsertOne(GuestMemory& memory, GuestAddress heap,
    GuestAddress block, FrameRegisters& frame)
{
    const GuestAddress cursor = InsertionCursor(memory, heap,
        memory.ReadU32(Slot(frame)) & 0xFFFFu, frame);
    const GuestAddress next = memory.ReadU32(cursor + 4u);
    const GuestAddress link = block + 8u;
    memory.WriteU32(block + 8u, cursor);
    memory.WriteU32(block + 12u, next);
    memory.WriteU32(next, link);
    memory.WriteU32(cursor + 4u, link);
    memory.WriteU32(heap + 48u, memory.ReadU32(heap + 48u) +
        memory.ReadU32(Slot(frame)));
}

std::uint64_t Resize(GuestMemory& memory,
    HeapSegmentServices& segments, HeapServices& coalesce,
    NativeServices& native, FrameRegisters& frame)
{
    const auto heap = [&] { return static_cast<GuestAddress>(R(frame, 30)); };
    const auto block = [&] { return static_cast<GuestAddress>(R(frame, 27)); };
    if (static_cast<GuestAddress>(R(frame, 29)) >
        memory.ReadU32(heap() + 28u))
        return 0;

    const std::uint32_t old_units = memory.ReadU16(block());
    R(frame, 25) = 1;
    R(frame, 26) = memory.ReadU8(block() + 5u);
    R(frame, 31) = R(frame, 27) + (old_units << 4u);
    std::uint64_t combined_units;
    if ((R(frame, 26) & 0x10u) != 0)
    {
        // A last block can grow only after committing a rounded segment page.
        const std::uint32_t difference =
            static_cast<GuestAddress>(R(frame, 29) - old_units);
        const std::uint32_t rounded = ((difference << 4u) + 0xFFFFu) &
            0xFFFF0000u;
        memory.WriteU32(Slot(frame), rounded);
        const auto segment = Segment(memory, heap(), block());
        frame.lr = 0x827CBD30u;
        const auto added = ExtendHeapSegment(memory, segments, heap(),
            segment, Slot(frame), static_cast<GuestAddress>(R(frame, 31)),
            static_cast<GuestAddress>(frame.sp - 160u));
        if (added == 0)
            return 0;
        memory.WriteU32(Slot(frame), memory.ReadU32(Slot(frame)) >> 4u);
        frame.lr = 0x827CBD54u;
        const auto merged = CoalesceFreeBlocks(memory, coalesce, heap(),
            added, Slot(frame), 0);
        const auto live_old_units = memory.ReadU16(block());
        combined_units = live_old_units + memory.ReadU32(Slot(frame));
        R(frame, 28) = memory.ReadU8(merged + 5u);
        if (static_cast<GuestAddress>(combined_units) <
            static_cast<GuestAddress>(R(frame, 29)))
        {
            frame.lr = 0x827CBD78u;
            InsertFreeBlocks(memory, heap(), merged,
                memory.ReadU32(Slot(frame)));
            memory.WriteU32(heap() + 48u,
                memory.ReadU32(heap() + 48u) + memory.ReadU32(Slot(frame)));
            return 0;
        }
    }
    else
    {
        R(frame, 28) = memory.ReadU8(static_cast<GuestAddress>(R(frame, 31)) + 5u);
        if ((R(frame, 28) & 1u) != 0)
            return 0;
        combined_units = old_units +
            memory.ReadU16(static_cast<GuestAddress>(R(frame, 31)));
        memory.WriteU32(Slot(frame), static_cast<GuestAddress>(combined_units));
        if (static_cast<GuestAddress>(combined_units) <
            static_cast<GuestAddress>(R(frame, 29)))
            return 0;
        UnlinkFreeBlock(memory, heap(), static_cast<GuestAddress>(R(frame, 31)), frame);
        CompareFreeFill(memory, native, frame,
            static_cast<GuestAddress>(R(frame, 31)), 0x827CBE4Cu);
        memory.WriteU32(heap() + 48u,
            memory.ReadU32(heap() + 48u) -
                memory.ReadU16(static_cast<GuestAddress>(R(frame, 31))));
        // The PPC reloads +80 after the native comparison; it may have
        // changed through guest-memory aliases during that call.
        combined_units = memory.ReadU32(Slot(frame));
    }

    R(frame, 24) = (std::uint32_t{memory.ReadU16(block())} << 4u) -
        memory.ReadU8(block() + 6u);
    std::uint64_t remainder = combined_units - R(frame, 29);
    memory.WriteU32(Slot(frame), static_cast<GuestAddress>(remainder));
    if (static_cast<GuestAddress>(remainder) <= 2u)
    {
        R(frame, 29) += remainder;
        remainder = 0;
        memory.WriteU32(Slot(frame), 0);
    }
    if ((R(frame, 26) & 2u) != 0)
    {
        // Copy the two live trailer words in their original load/store order.
        const auto old_end = static_cast<GuestAddress>(
            R(frame, 27) +
            (std::uint32_t{memory.ReadU16(block())} << 4u));
        const auto new_end = static_cast<GuestAddress>(
            R(frame, 27) + (static_cast<GuestAddress>(R(frame, 29)) << 4u));
        WriteU64(memory, new_end - 16u, ReadU64(memory, old_end - 16u));
        WriteU64(memory, new_end - 8u, ReadU64(memory, old_end - 8u));
    }
    memory.WriteU16(block(), static_cast<std::uint16_t>(R(frame, 29)));

    if (remainder == 0)
    {
        const auto merged_flags = memory.ReadU8(block() + 5u) |
            static_cast<std::uint8_t>(R(frame, 28) & 0x10u);
        memory.WriteU8(block() + 5u, merged_flags);
        memory.WriteU8(block() + 6u, static_cast<std::uint8_t>(
            (static_cast<GuestAddress>(R(frame, 29)) << 4u) - R(frame, 23)));
        if ((R(frame, 28) & 0x10u) != 0)
            SetLast(memory, heap(), block(), block());
        else
            memory.WriteU16(static_cast<GuestAddress>(
                R(frame, 27) + (static_cast<GuestAddress>(R(frame, 29)) << 4u) + 2u),
                static_cast<std::uint16_t>(R(frame, 29)));
    }
    else
    {
        R(frame, 31) = R(frame, 27) +
            (static_cast<GuestAddress>(R(frame, 29)) << 4u);
        const auto free_block = [&] { return static_cast<GuestAddress>(R(frame, 31)); };
        memory.WriteU8(block() + 6u, static_cast<std::uint8_t>(
            (static_cast<GuestAddress>(R(frame, 29)) << 4u) - R(frame, 23)));
        memory.WriteU16(free_block() + 2u,
            static_cast<std::uint16_t>(R(frame, 29)));
        memory.WriteU8(free_block() + 4u, memory.ReadU8(block() + 4u));
        if ((R(frame, 28) & 0x10u) != 0)
        {
            SetLast(memory, heap(), block(), free_block());
            memory.WriteU8(free_block() + 5u,
                static_cast<std::uint8_t>(R(frame, 28)));
            memory.WriteU8(free_block() + 5u,
                static_cast<std::uint8_t>(R(frame, 28) & 0xF8u));
            memory.WriteU16(free_block(),
                static_cast<std::uint16_t>(memory.ReadU32(Slot(frame))));
            InsertOne(memory, heap(), free_block(), frame);
        }
        else
        {
            const std::uint8_t preceding_flags =
                static_cast<std::uint8_t>(R(frame, 28));
            R(frame, 29) = R(frame, 31) +
                (memory.ReadU32(Slot(frame)) << 4u);
            R(frame, 28) = memory.ReadU8(static_cast<GuestAddress>(R(frame, 29)) + 5u);
            if ((R(frame, 28) & 1u) != 0)
            {
                memory.WriteU8(free_block() + 5u, preceding_flags & 0xEFu);
                memory.WriteU16(free_block(),
                    static_cast<std::uint16_t>(memory.ReadU32(Slot(frame))));
                memory.WriteU16(static_cast<GuestAddress>(R(frame, 29)) + 2u,
                    static_cast<std::uint16_t>(memory.ReadU32(Slot(frame))));
                memory.WriteU8(free_block() + 5u,
                    memory.ReadU8(free_block() + 5u) & 0xF8u);
                InsertOne(memory, heap(), free_block(), frame);
            }
            else
            {
                const GuestAddress following = static_cast<GuestAddress>(R(frame, 29));
                UnlinkFreeBlock(memory, heap(), following, frame);
                CompareFreeFill(memory, native, frame, following, 0x827CC0C0u);
                const auto following_units_before =
                    memory.ReadU16(static_cast<GuestAddress>(R(frame, 29)));
                const auto free_units_before = memory.ReadU32(heap() + 48u);
                const std::uint32_t updated_free_units =
                    free_units_before - following_units_before;
                const std::uint32_t existing_remainder =
                    memory.ReadU32(Slot(frame));
                memory.WriteU32(heap() + 48u, updated_free_units);
                const auto following_units_after =
                    memory.ReadU16(static_cast<GuestAddress>(R(frame, 29)));
                memory.WriteU32(Slot(frame),
                    existing_remainder + following_units_after);
                memory.WriteU8(free_block() + 5u,
                    static_cast<std::uint8_t>(R(frame, 28)));
                if (memory.ReadU32(Slot(frame)) > 0xF000u)
                {
                    frame.lr = 0x827CC1DCu;
                    InsertFreeBlocks(memory, heap(), free_block(),
                        memory.ReadU32(Slot(frame)));
                }
                else
                {
                    memory.WriteU16(free_block(),
                        static_cast<std::uint16_t>(memory.ReadU32(Slot(frame))));
                    if ((R(frame, 28) & 0x10u) != 0)
                        SetLast(memory, heap(), block(), free_block());
                    else
                        memory.WriteU16(free_block() +
                            (memory.ReadU32(Slot(frame)) << 4u) + 2u,
                            static_cast<std::uint16_t>(memory.ReadU32(Slot(frame))));
                    memory.WriteU8(free_block() + 5u,
                        memory.ReadU8(free_block() + 5u) & 0xF8u);
                    InsertOne(memory, heap(), free_block(), frame);
                }
            }
        }
    }

    if ((R(frame, 22) & 8u) != 0 &&
        static_cast<GuestAddress>(R(frame, 23)) >
            static_cast<GuestAddress>(R(frame, 24)))
    {
        frame.lr = 0x827CC200u;
        (void)FillGuestMemory(memory,
            static_cast<GuestAddress>(R(frame, 27) + R(frame, 24) + 16u),
            0, static_cast<GuestAddress>(R(frame, 23) - R(frame, 24)));
    }
    const auto existing = memory.ReadU8(block() + 5u);
    const auto requested_flags = std::rotl(static_cast<std::uint32_t>(
        R(frame, 22)), 28u) & 0xE0u;
    memory.WriteU8(block() + 5u, static_cast<std::uint8_t>(
        (existing & ~0xE0u) | requested_flags));
    return 1;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    HeapSegmentServices& segment_services,
    HeapServices& coalesce_services, NativeServices& native_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t incoming_r5, std::uint64_t incoming_r6,
    std::uint64_t incoming_r7, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != kAddress)
        return false;
    Enter(memory, caller_sp, frame);
    R(frame, 30) = incoming_r3;
    R(frame, 29) = incoming_r7;
    R(frame, 22) = incoming_r4;
    R(frame, 27) = incoming_r5;
    R(frame, 23) = incoming_r6;
    result = Resize(memory, segment_services, coalesce_services,
        native_services, frame);
    Leave(memory, frame);
    return true;
}
} // namespace lo::semantic::gpu::heap_block_resize
