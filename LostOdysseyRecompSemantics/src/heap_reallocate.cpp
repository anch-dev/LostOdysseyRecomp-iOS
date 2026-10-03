#include "lo_semantics/heap_reallocate.h"

#include "lo_semantics/heap.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/memory_move.h"

#include <algorithm>
#include <array>
#include <bit>

namespace lo::semantic::gpu::heap_reallocate
{
namespace
{
GuestAddress Address(std::uint64_t value) { return static_cast<GuestAddress>(value); }
std::uint32_t Low(std::uint64_t value) { return static_cast<std::uint32_t>(value); }
std::uint64_t DuplicateLow(std::uint64_t value)
{ return std::uint64_t{Low(value)} | (std::uint64_t{Low(value)} << 32); }
std::uint32_t U32(GuestMemory& memory, std::uint64_t address)
{ return memory.ReadU32(Address(address)); }
std::uint16_t U16(GuestMemory& memory, std::uint64_t address)
{ return memory.ReadU16(Address(address)); }
std::uint8_t U8(GuestMemory& memory, std::uint64_t address)
{ return memory.ReadU8(Address(address)); }
void W32(GuestMemory& memory, std::uint64_t address, std::uint64_t value)
{ memory.WriteU32(Address(address), Low(value)); }
void W16(GuestMemory& memory, std::uint64_t address, std::uint64_t value)
{ memory.WriteU16(Address(address), static_cast<std::uint16_t>(value)); }
void W8(GuestMemory& memory, std::uint64_t address, std::uint64_t value)
{ memory.WriteU8(Address(address), static_cast<std::uint8_t>(value)); }
std::uint64_t U64(GuestMemory& memory, std::uint64_t address)
{ return (std::uint64_t{U32(memory, address)} << 32) | U32(memory, address + 4); }
void W64(GuestMemory& memory, std::uint64_t address, std::uint64_t value)
{ W32(memory, address, value >> 32); W32(memory, address + 4, value); }

void CompareUnsigned(Condition& condition, std::uint32_t left,
    std::uint32_t right, std::uint8_t so)
{
    condition = {static_cast<std::uint8_t>(left < right),
        static_cast<std::uint8_t>(left > right),
        static_cast<std::uint8_t>(left == right), so};
}
void CompareSigned(Condition& condition, std::uint32_t left,
    std::uint32_t right, std::uint8_t so)
{
    const auto a = static_cast<std::int32_t>(left);
    const auto b = static_cast<std::int32_t>(right);
    condition = {static_cast<std::uint8_t>(a < b),
        static_cast<std::uint8_t>(a > b),
        static_cast<std::uint8_t>(a == b), so};
}

std::array<std::uint64_t*, 13> Nonvolatile(Registers& r)
{
    return {&r.r19, &r.r20, &r.r21, &r.r22, &r.r23, &r.r24, &r.r25,
        &r.r26, &r.r27, &r.r28, &r.r29, &r.r30, &r.r31};
}

void SaveParent(GuestMemory& memory, Registers& r)
{
    const auto incoming_sp = r.sp;
    auto saves = Nonvolatile(r);
    for (std::size_t i = 0; i < saves.size(); ++i)
        W64(memory, incoming_sp - 112 + i * 8, *saves[i]);
    r.r12 = r.lr;
    W32(memory, incoming_sp - 8, r.r12);
    r.lr = 0x827ccf88u;
    r.r31 = incoming_sp - 320;
    W32(memory, r.r31, incoming_sp);
    r.sp = r.r31;
}

void RestoreParent(GuestMemory& memory, Registers& r)
{
    r.sp = r.r31 + 320;
    auto saves = Nonvolatile(r);
    for (std::size_t i = 0; i < saves.size(); ++i)
        *saves[i] = U64(memory, r.sp - 112 + i * 8);
    r.r12 = U32(memory, r.sp - 8);
    r.lr = r.r12;
}

class Machine
{
public:
    Machine(GuestMemory& memory, Services& services, Registers& r)
        : memory_(memory), services_(services), r_(r) {}

    void Run()
    {
        SaveParent(memory_, r_);
        r_.r27 = r_.r3;
        W32(memory_, Frame() + 340, r_.r27);
        r_.r30 = r_.r4;
        r_.r20 = r_.r5;
        W32(memory_, Frame() + 356, r_.r20);
        r_.r26 = r_.r6;
        W32(memory_, Frame() + 364, r_.r26);
        r_.r22 = r_.r27;
        W32(memory_, Frame() + 84, r_.r22);
        r_.r19 = 0;
        W32(memory_, Frame() + 96, 0);

        r_.r11 = U32(memory_, r_.r27 + 20) & 0x40000u;
        CompareSigned(r_.cr0, Low(r_.r11), 0, r_.xer_so);
        if (!r_.cr0.eq)
        {
            r_.lr = 0x827ccfccu;
            services_.GetCurrentProcessType(memory_, r_);
            r_.r11 = U8(memory_, r_.r27 + 379);
            CompareSigned(r_.cr6, Low(r_.r11), Low(r_.r3), r_.xer_so);
            if (!r_.cr6.eq)
            {
                r_.r7 = r_.r20;
                r_.r6 = 3144;
                r_.r5 = U32(memory_, Frame() + 312);
                r_.r4 = r_.r27;
                r_.r3 = 244;
                r_.lr = 0x827ccff0u;
                services_.BugCheck(memory_, r_);
            }
        }

        CompareUnsigned(r_.cr6, Low(r_.r20), 0, r_.xer_so);
        if (r_.cr6.eq)
        {
            r_.r3 = 0;
            RestoreParent(memory_, r_);
            return;
        }

        r_.r11 = U32(memory_, r_.r27 + 24);
        r_.r10 = 0x7fffffffu;
        r_.r23 = r_.r11 | r_.r30;
        CompareUnsigned(r_.cr6, Low(r_.r26), 0x7fffffffu, r_.xer_so);
        if (r_.cr6.gt)
        {
            r_.r3 = 0;
            RestoreParent(memory_, r_);
            return;
        }
        CompareUnsigned(r_.cr6, Low(r_.r26), 0, r_.xer_so);
        r_.r21 = 1;
        r_.r11 = r_.r26;
        if (r_.cr6.eq) r_.r11 = 1;
        r_.r10 = U32(memory_, r_.r27 + 80);
        r_.r9 = Low(r_.r23) & 0x3fffff00u;
        r_.r9 &= 0xfc0001ffu;
        CompareSigned(r_.cr0, Low(r_.r9), 0, r_.xer_so);
        r_.r8 = U32(memory_, r_.r27 + 84);
        r_.r11 = r_.r10 + r_.r11;
        r_.r24 = r_.r11 & r_.r8;
        W32(memory_, Frame() + 80, r_.r24);
        if (!r_.cr0.eq)
        {
            r_.r24 += 16;
            W32(memory_, Frame() + 80, r_.r24);
        }
        else
        {
            r_.r11 = U32(memory_, r_.r27 + 380);
            CompareUnsigned(r_.cr6, Low(r_.r11), 0, r_.xer_so);
            if (r_.cr6.eq)
            {
                r_.r11 = U8(memory_, r_.r20 - 11) & 2u;
                CompareSigned(r_.cr0, Low(r_.r11), 0, r_.xer_so);
            }
            if (!r_.cr6.eq || !r_.cr0.eq)
            {
                r_.r24 += 16;
                W32(memory_, Frame() + 80, r_.r24);
            }
        }

        r_.r11 = Low(r_.r23) & 1u;
        CompareSigned(r_.cr0, Low(r_.r11), 0, r_.xer_so);
        if (r_.cr0.eq)
        {
            r_.r3 = U32(memory_, r_.r27 + 1408);
            r_.lr = 0x827cd084u;
            services_.EnterCriticalSection(memory_, r_);
            W32(memory_, Frame() + 96, r_.r21);
            r_.r23 ^= 1u;
            W32(memory_, Frame() + 348, r_.r23);
        }
        r_.r30 = r_.r20 - 16;
        W32(memory_, Frame() + 124, r_.r30);
        r_.r8 = U8(memory_, r_.r30 + 5);
        r_.r11 = Low(r_.r8) & 1u;
        CompareSigned(r_.cr0, Low(r_.r11), 0, r_.xer_so);
        if (!r_.cr0.eq)
        {
            r_.r7 = Low(r_.r8) & 8u;
            CompareSigned(r_.cr0, Low(r_.r7), 0, r_.xer_so);
            r_.r11 = U16(memory_, r_.r30);
            if (!r_.cr0.eq)
            {
                r_.r9 = U32(memory_, r_.r30 - 8);
                r_.r6 = r_.r9;
                r_.r10 = r_.r24 + 32;
                W32(memory_, Frame() + 80, r_.r10);
                r_.r9 -= r_.r11;
                r_.r25 = r_.r9 - 48;
                r_.r29 = std::rotr(Low(r_.r6), 4) & 0x0fffffffu;
                r_.r24 = Low(r_.r10 + 65535u) & 0xffff0000u;
                W32(memory_, Frame() + 80, r_.r24);
            }
            else
            {
                r_.r10 = U8(memory_, r_.r30 + 6);
                r_.r29 = r_.r11;
                r_.r9 = Low(r_.r29 << 4) & 0xfffffff0u;
                r_.r25 = r_.r9 - r_.r10;
            }
            W32(memory_, Frame() + 92, r_.r25);
            r_.r28 = std::rotr(Low(r_.r24), 4) & 0x0fffffffu;
            W32(memory_, Frame() + 100, r_.r28);
            CompareUnsigned(r_.cr6, Low(r_.r28), Low(r_.r29), r_.xer_so);
            if (r_.cr6.gt) GrowOrMove();
            else Shrink();
        }
        Finish();
    }

private:
    GuestAddress Frame() const { return Address(r_.r31); }
    void SaveNested(unsigned first, std::uint32_t frame_bytes)
    {
        auto registers = Nonvolatile(r_);
        for (unsigned reg = first; reg <= 31; ++reg)
            W64(memory_, r_.sp - 8u * (33u - reg), *registers[reg - 19u]);
        W32(memory_, r_.sp - 8u, r_.lr);
        W32(memory_, r_.sp - frame_bytes, r_.sp);
    }
    void RestoreNested(unsigned first)
    {
        auto registers = Nonvolatile(r_);
        for (unsigned reg = first; reg <= 31; ++reg)
            *registers[reg - 19u] = U64(memory_, r_.sp - 8u * (33u - reg));
        r_.r12 = U32(memory_, r_.sp - 8u);
        r_.lr = r_.r12;
    }
    void InsertRemainder(GuestAddress block, std::uint32_t units,
        std::uint32_t added_units, std::uint32_t cursor_slot)
    {
        const auto heap = Address(r_.r27);
        const auto head = units < 128u ? heap + (units + 48u) * 8u : heap + 384u;
        GuestAddress cursor = head;
        if (units < 128u)
        {
            cursor = memory_.ReadU32(head);
            if (cursor == head)
            {
                const auto n = memory_.ReadU16(block);
                const auto bitmap = heap + ((n >> 5) + 88u) * 4u;
                memory_.WriteU32(bitmap, memory_.ReadU32(bitmap) |
                    (Low(r_.r21) << (n & 31u)));
            }
            cursor = head;
        }
        else
        {
            cursor = memory_.ReadU32(head);
            W32(memory_, Frame() + cursor_slot, cursor);
            while (cursor != head && units > memory_.ReadU16(cursor - 8u))
            {
                cursor = memory_.ReadU32(cursor);
                W32(memory_, Frame() + cursor_slot, cursor);
            }
        }
        const auto previous = memory_.ReadU32(cursor + 4u);
        const auto link = block + 8u;
        memory_.WriteU32(link, cursor);
        memory_.WriteU32(link + 4u, previous);
        memory_.WriteU32(previous, link);
        memory_.WriteU32(cursor + 4u, link);
        memory_.WriteU32(heap + 48u, memory_.ReadU32(heap + 48u) + added_units);
    }

    void UnlinkNext(GuestAddress block)
    {
        const auto previous = U32(memory_, block + 12);
        const auto next = U32(memory_, block + 8);
        const auto observed_next = U32(memory_, previous);
        const auto observed_previous = U32(memory_, next + 4);
        CompareUnsigned(r_.cr6, observed_next, observed_previous, r_.xer_so);
        if (!r_.cr6.eq) return;
        CompareUnsigned(r_.cr6, observed_next, block + 8u, r_.xer_so);
        if (!r_.cr6.eq) return;
        CompareUnsigned(r_.cr6, next, previous, r_.xer_so);
        W32(memory_, previous, next);
        W32(memory_, next + 4, previous);
        if (r_.cr6.eq)
        {
            const auto units = U16(memory_, block);
            CompareUnsigned(r_.cr6, units, 128u, r_.xer_so);
            if (r_.cr6.lt)
            {
                const auto bitmap = Address(r_.r27) + ((units >> 5) + 88u) * 4u;
                W32(memory_, bitmap, U32(memory_, bitmap) ^
                    (Low(r_.r21) << (units & 31u)));
            }
        }
    }

    void Shrink()
    {
        r_.r10 = r_.r28 + 1;
        CompareUnsigned(r_.cr6, Low(r_.r10), Low(r_.r29), r_.xer_so);
        if (r_.cr6.eq)
        {
            r_.r28 = r_.r10;
            W32(memory_, Frame() + 100, r_.r28);
            r_.r24 += 16;
            W32(memory_, Frame() + 80, r_.r24);
        }
        CompareUnsigned(r_.cr6, Low(r_.r7), 0, r_.xer_so);
        if (!r_.cr6.eq)
            W16(memory_, r_.r30, r_.r24 - r_.r26 + 65536u - 48u);
        else if ((Low(r_.r8) & 2u) != 0)
        {
            const auto old_tail = r_.r30 + (Low(r_.r11) << 4);
            const auto new_tail = r_.r30 + (Low(r_.r28) << 4);
            W64(memory_, new_tail - 16, U64(memory_, old_tail - 16));
            W64(memory_, new_tail - 8, U64(memory_, old_tail - 8));
            W8(memory_, r_.r30 + 6, r_.r24 - r_.r26);
        }
        else W8(memory_, r_.r30 + 6, r_.r24 - r_.r26);

        CompareUnsigned(r_.cr6, Low(r_.r26), Low(r_.r25), r_.xer_so);
        if (r_.cr6.gt && (Low(r_.r23) & 8u) != 0)
        {
            r_.r5 = r_.r26 - r_.r25;
            r_.r4 = 0;
            r_.r3 = r_.r25 + r_.r20;
            r_.lr = 0x827cd1a4u;
            r_.r3 = FillGuestMemory(memory_, Address(r_.r3), 0, Low(r_.r5));
        }
        CompareUnsigned(r_.cr6, Low(r_.r28), Low(r_.r29), r_.xer_so);
        if (r_.cr6.eq) return;

        const auto old_flags = U8(memory_, r_.r30 + 5);
        if ((old_flags & 8u) != 0)
        {
            r_.r30 -= 32;
            r_.r6 = 0;
            r_.r5 = 32768;
            r_.r4 = Frame() + 88;
            r_.r3 = Frame() + 104;
            W32(memory_, Frame() + 104, r_.r30 + r_.r24);
            W32(memory_, Frame() + 88, (Low(r_.r29) << 4) - Low(r_.r24));
            r_.lr = 0x827cd1f0u;
            services_.FreeVirtualMemory(memory_, r_);
            CompareSigned(r_.cr0, Low(r_.r3), 0, r_.xer_so);
            if (!r_.cr0.lt)
                W32(memory_, r_.r30 + 24,
                    U32(memory_, r_.r30 + 24) - U32(memory_, Frame() + 88));
            return;
        }

        const auto block = Address(r_.r30);
        const auto requested_units = Low(r_.r28);
        r_.r29 = r_.r30 + (requested_units << 4);
        W8(memory_, r_.r29 + 5, old_flags & ~1u);
        W16(memory_, r_.r29 + 2, requested_units);
        W8(memory_, r_.r29 + 4, U8(memory_, block + 4));
        r_.r28 = U16(memory_, block) - r_.r28;
        W16(memory_, block, requested_units);
        W8(memory_, block + 5, U8(memory_, block + 5) & ~0x10u);

        if ((old_flags & 0x10u) != 0)
        {
            const auto segment = U32(memory_, Address(r_.r27) +
                (U8(memory_, r_.r29 + 4) + 24u) * 4u);
            W32(memory_, segment + 64, r_.r29);
            W16(memory_, r_.r29, r_.r28);
            W8(memory_, r_.r29 + 5, U8(memory_, r_.r29 + 5) & ~7u);
            InsertRemainder(Address(r_.r29), Low(r_.r28), Low(r_.r28), 108);
            return;
        }

        r_.r30 = r_.r29 + (Low(r_.r28) << 4);
        const auto next_flags = U8(memory_, r_.r30 + 5);
        if ((next_flags & 1u) != 0)
        {
            W16(memory_, r_.r29, r_.r28);
            W16(memory_, r_.r30 + 2, r_.r28);
            W8(memory_, r_.r29 + 5, U8(memory_, r_.r29 + 5) & ~7u);
            InsertRemainder(Address(r_.r29), Low(r_.r28), Low(r_.r28),
                Low(r_.r28) < 128u ? 0u : 112u);
            return;
        }

        W8(memory_, r_.r29 + 5, next_flags);
        UnlinkNext(Address(r_.r30));
        if ((next_flags & 4u) != 0)
        {
            const auto units = U16(memory_, r_.r30);
            r_.r11 = std::rotl(std::uint32_t{units}, 4);
            r_.r4 = r_.r11 - 24u;
            W32(memory_, Frame() + 116, r_.r4);
            if ((next_flags & 2u) != 0 && Low(r_.r4) > 4u)
            {
                r_.r4 -= 4;
                W32(memory_, Frame() + 116, r_.r4);
            }
            r_.r5 = 0xfffffffffeeefeeeull;
            r_.r3 = r_.r30 + 24;
            r_.lr = 0x827cd420u;
            services_.CompareMemoryUlong(memory_, r_);
        }
        W32(memory_, Address(r_.r27) + 48,
            U32(memory_, Address(r_.r27) + 48) - U16(memory_, r_.r30));
        r_.r5 = U16(memory_, r_.r30) + r_.r28;
        CompareUnsigned(r_.cr6, Low(r_.r5), 61440u, r_.xer_so);
        if (r_.cr6.gt)
        {
            r_.r4 = r_.r29;
            r_.r3 = r_.r27;
            r_.lr = 0x827cd534u;
            InsertFreeBlocks(memory_, Address(r_.r3), Address(r_.r4), Low(r_.r5));
            return;
        }
        W16(memory_, r_.r29, r_.r5);
        if ((U8(memory_, r_.r29 + 5) & 0x10u) == 0)
            W16(memory_, r_.r29 + (Low(r_.r5) << 4) + 2, r_.r5);
        else
        {
            const auto segment = U32(memory_, Address(r_.r27) +
                (U8(memory_, r_.r29 + 4) + 24u) * 4u);
            W32(memory_, segment + 64, r_.r29);
        }
        W8(memory_, r_.r29 + 5, U8(memory_, r_.r29 + 5) & ~7u);
        InsertRemainder(Address(r_.r29), Low(r_.r5), Low(r_.r5), 120);
    }

    void GrowOrMove()
    {
        CompareUnsigned(r_.cr6, Low(r_.r7), 0, r_.xer_so);
        if (r_.cr6.eq)
        {
            r_.r7 = r_.r28;
            r_.r6 = r_.r26;
            r_.r5 = r_.r30;
            r_.r4 = r_.r23;
            r_.r3 = r_.r27;
            r_.lr = 0x827cd558u;
            heap_block_resize::FrameRegisters frame{};
            frame.lr = r_.lr;
            frame.sp = r_.sp;
            frame.r22_through_r31 = {r_.r22, r_.r23, r_.r24, r_.r25, r_.r26,
                r_.r27, r_.r28, r_.r29, r_.r30, r_.r31};
            std::uint64_t result = 0;
            (void)heap_block_resize::Apply(0x827cbca8u, memory_, services_.Segment(),
                services_.Free(), services_.ResizeNative(), r_.r3, r_.r4, r_.r5,
                r_.r6, r_.r7, r_.sp, frame, result);
            r_.r3 = result;
            r_.sp = frame.sp;
            r_.lr = frame.lr;
            r_.r22 = frame.r22_through_r31[0]; r_.r23 = frame.r22_through_r31[1];
            r_.r24 = frame.r22_through_r31[2]; r_.r25 = frame.r22_through_r31[3];
            r_.r26 = frame.r22_through_r31[4]; r_.r27 = frame.r22_through_r31[5];
            r_.r28 = frame.r22_through_r31[6]; r_.r29 = frame.r22_through_r31[7];
            r_.r30 = frame.r22_through_r31[8]; r_.r31 = frame.r22_through_r31[9];
            CompareUnsigned(r_.cr0, Low(r_.r3), 0, r_.xer_so);
            if (!r_.cr0.eq) return;
        }
        if ((Low(r_.r23) & 0x10u) != 0)
        {
            W32(memory_, Frame() + 356, r_.r19);
            forced_failure_ = true;
            return;
        }
        MoveBlock();
    }

    void MoveBlock()
    {
        r_.r23 = DuplicateLow(r_.r23) & 0xffffffffc003ffffull;
        W32(memory_, Frame() + 348, r_.r23);
        const auto pre_allocate_flags = U8(memory_, r_.r30 + 5);
        if ((pre_allocate_flags & 2u) != 0)
        {
            r_.r10 = DuplicateLow(r_.r23) & 0xfffffffffffff1ffull;
            r_.r9 = 256u | ((std::uint32_t{pre_allocate_flags} << 4) & 0xe00u);
            r_.r23 = r_.r9 | r_.r10;
            W32(memory_, Frame() + 348, r_.r23);
            const auto meta = (pre_allocate_flags & 8u) != 0
                ? r_.r30 - 24 : r_.r30 + (U16(memory_, r_.r30) << 4) - 16;
            r_.r11 = U16(memory_, meta + 2);
            CompareUnsigned(r_.cr0, Low(r_.r11), 0, r_.xer_so);
            if (!r_.cr0.eq)
            {
                r_.r10 = Low(r_.r11) & 0x8000u;
                CompareSigned(r_.cr0, Low(r_.r10), 0, r_.xer_so);
                if (r_.cr0.eq)
                {
                    r_.r23 |= (Low(r_.r11) << 18) & 0xfffc0000u;
                    W32(memory_, Frame() + 348, r_.r23);
                }
            }
        }
        else
        {
            r_.r11 = U8(memory_, r_.r30 + 7);
            CompareUnsigned(r_.cr0, Low(r_.r11), 0, r_.xer_so);
            if (!r_.cr0.eq)
            {
                r_.r23 |= (Low(r_.r11) << 18) & 0xfffc0000u;
                W32(memory_, Frame() + 348, r_.r23);
            }
        }
        r_.r5 = r_.r26;
        r_.r4 = DuplicateLow(r_.r23) & 0xfffffffffffffff7ull;
        r_.r3 = r_.r27;
        r_.lr = 0x827cd660u;
        const auto child_frame = Address(r_.sp - 320);
        SaveNested(22, 320);
        r_.r3 = AllocateHeapBlock(memory_, services_.Allocation(), Address(r_.r3),
            Low(r_.r4), Low(r_.r5), child_frame);
        RestoreNested(22);
        r_.r29 = r_.r3;
        CompareSigned(r_.cr0, Low(r_.r29), 0, r_.xer_so);
        if (!r_.cr0.eq)
        {
            const auto new_header = r_.r29 - 16;
            const auto new_flags = U8(memory_, new_header + 5);
            if ((new_flags & 2u) != 0)
            {
                const auto new_meta = (new_flags & 8u) != 0
                    ? new_header - 24 : new_header + (U16(memory_, new_header) << 4) - 16;
                const auto after_allocate_flags = U8(memory_, r_.r30 + 5);
                if ((after_allocate_flags & 2u) != 0)
                {
                    const auto old_meta = (after_allocate_flags & 8u) != 0
                        ? r_.r30 - 24 : r_.r30 + (U16(memory_, r_.r30) << 4) - 16;
                    W32(memory_, new_meta + 4, U32(memory_, old_meta + 4));
                }
                else
                {
                    W64(memory_, new_meta, r_.r19);
                    W64(memory_, new_meta + 8, r_.r19);
                }
            }
            CompareUnsigned(r_.cr6, Low(r_.r26), Low(r_.r25), r_.xer_so);
            r_.r5 = r_.cr6.lt ? r_.r26 : r_.r25;
            r_.r4 = r_.r20;
            r_.r3 = r_.r29;
            r_.lr = 0x827cd6fcu;
            r_.r3 = MoveGuestMemory(memory_, r_.r3, Address(r_.r4),
                r_.r5, Address(r_.sp));
            CompareUnsigned(r_.cr6, Low(r_.r26), Low(r_.r25), r_.xer_so);
            if (r_.cr6.gt && (Low(r_.r23) & 8u) != 0)
            {
                r_.r5 = r_.r26 - r_.r25;
                r_.r4 = 0;
                r_.r3 = r_.r29 + r_.r25;
                r_.lr = 0x827cd71cu;
                r_.r3 = FillGuestMemory(memory_, Address(r_.r3), 0, Low(r_.r5));
            }
            r_.r5 = r_.r20;
            r_.r4 = r_.r23;
            r_.r3 = r_.r27;
            r_.lr = 0x827cd72cu;
            const auto free_frame = Address(r_.sp - 176);
            SaveNested(25, 176);
            r_.r3 = FreeHeapBlock(memory_, services_.Free(), Address(r_.r3),
                Low(r_.r4), Address(r_.r5), free_frame);
            RestoreNested(25);
        }
        r_.r20 = r_.r29;
        W32(memory_, Frame() + 356, r_.r20);
    }

    void Finish()
    {
        if (!forced_failure_)
            CompareUnsigned(r_.cr6, Low(r_.r20), 0, r_.xer_so);
        if (forced_failure_ || r_.cr6.eq)
        {
            r_.r11 = Low(r_.r23) & 4u;
            CompareSigned(r_.cr0, Low(r_.r11), 0, r_.xer_so);
            if (!r_.cr0.eq)
            {
                r_.r11 = 0xffffffffc0000017ull;
                W32(memory_, Frame() + 128, r_.r11);
                W32(memory_, Frame() + 136, r_.r19);
                W32(memory_, Frame() + 144, r_.r21);
                r_.r3 = Frame() + 128;
                W32(memory_, Frame() + 132, r_.r19);
                W32(memory_, Frame() + 148, r_.r24);
                r_.lr = 0x827cd768u;
                services_.RaiseException(memory_, r_);
            }
        }
        r_.r12 = r_.r31 + 320;
        r_.lr = 0x827cd790u;
        heap_lock_exit::Registers exit{};
        exit.sp = r_.sp; exit.lr = r_.lr; exit.r3 = r_.r3;
        exit.r11 = r_.r11; exit.r12 = r_.r12;
        exit.r22 = r_.r22; exit.r31 = r_.r31;
        exit.xer_so = r_.xer_so;
        exit.cr6 = {r_.cr6.lt, r_.cr6.gt, r_.cr6.eq, r_.cr6.so};
        (void)heap_lock_exit::Apply(0x827cd7bcu, memory_,
            services_.LockExitNative(), exit);
        r_.sp = exit.sp; r_.lr = exit.lr; r_.r3 = exit.r3;
        r_.r11 = exit.r11; r_.r12 = exit.r12;
        r_.r22 = exit.r22; r_.r31 = exit.r31;
        r_.cr6 = {exit.cr6.lt, exit.cr6.gt, exit.cr6.eq, exit.cr6.so};
        r_.r3 = U32(memory_, Frame() + 356);
        RestoreParent(memory_, r_);
    }

    GuestMemory& memory_;
    Services& services_;
    Registers& r_;
    bool forced_failure_ = false;
};
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    Services& services, Registers& registers)
{
    if (address != 0x827ccf80u) return false;
    Machine(memory, services, registers).Run();
    return true;
}
} // namespace lo::semantic::gpu::heap_reallocate
