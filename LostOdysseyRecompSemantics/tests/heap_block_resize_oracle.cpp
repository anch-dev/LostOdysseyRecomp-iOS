#include "lo_semantics/heap_block_resize.h"
#include "lo_semantics/memory_fill.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::heap_block_resize;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Heap = 0x10000u, Block = 0x20000u;
constexpr GuestAddress Segment = 0x30000u;
constexpr std::uint64_t Stack = 0x1122334400380000ull;
enum class Mode { Limit, NeighborBusy, NeighborShort, GrowExact,
    ShrinkSplit, MergeFollowing, MergeCompare, NativeAlias, SegmentMissing };
struct Case { Mode mode; std::uint64_t expected; };
constexpr Case Cases[] = {
    {Mode::Limit, 0}, {Mode::NeighborBusy, 0},
    {Mode::NeighborShort, 0}, {Mode::GrowExact, 1},
    {Mode::ShrinkSplit, 1}, {Mode::MergeFollowing, 1},
    {Mode::MergeCompare, 1},
    {Mode::NativeAlias, 1}, {Mode::SegmentMissing, 0},
};
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x400000u, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve heap resize guest memory");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
using Event = std::array<std::uint64_t, 5>;
struct Services final : HeapSegmentServices, HeapServices, family::NativeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(Window& window, Mode selected)
        : memory(0, std::span<std::uint8_t>(window.bytes, Space)), mode(selected) {}
    std::int32_t CommitRange(GuestAddress, GuestAddress,
        GuestAddress, GuestAddress) override
    { throw std::runtime_error("unexpected segment commit callback"); }
    std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected virtual allocation"); }
    void InitializeUncommittedRange(GuestAddress, GuestAddress,
        std::uint32_t) override
    { throw std::runtime_error("unexpected uncommitted range"); }
    std::uint32_t CompareMemoryUlong(GuestAddress source,
        std::uint32_t bytes, std::uint32_t pattern) override
    {
        events.push_back({2, source, bytes, pattern});
        return bytes;
    }
    std::uint64_t CompareMemoryUlong(GuestMemory& guest,
        GuestAddress source, std::uint32_t bytes, std::uint32_t pattern,
        std::uint64_t caller_sp, family::FrameRegisters& frame) override
    {
        events.push_back({1, source, bytes, pattern, caller_sp});
        if (frame.sp != caller_sp)
            throw std::runtime_error("incorrect live SP at native compare");
        if (mode == Mode::NativeAlias)
        {
            const GuestAddress save = static_cast<GuestAddress>(Stack) - 16u;
            guest.WriteU32(save, 0xAABBCCDDu);
            guest.WriteU32(save + 4u, 0x12345678u);
            guest.WriteU32(static_cast<GuestAddress>(frame.sp + 80u), 9u);
            guest.WriteU16(Block, 3u);
        }
        if (mode == Mode::MergeCompare)
        {
            frame.r22_through_r31[25u - 22u] = 3u;
            frame.r22_through_r31[29u - 22u] = 0x6655443300022000ull;
            // The new +80 stack slot aliases the heap's free-unit counter.
            frame.sp = (Stack & 0xFFFFFFFF00000000ull) | (Heap - 32u);
        }
        return 0;
    }
};
Services* active = nullptr;

std::array<PPCRegister*, 10> Nonvolatile(PPCContext& context)
{
    return {&context.r22, &context.r23, &context.r24, &context.r25,
        &context.r26, &context.r27, &context.r28, &context.r29,
        &context.r30, &context.r31};
}
family::FrameRegisters Frame(PPCContext& context)
{
    family::FrameRegisters frame{context.lr, context.r1.u64, {}};
    const auto regs = Nonvolatile(context);
    for (std::size_t i = 0; i != regs.size(); ++i)
        frame.r22_through_r31[i] = regs[i]->u64;
    return frame;
}
void ToPpc(PPCContext& context, const family::FrameRegisters& frame)
{
    context.lr = frame.lr;
    context.r1.u64 = frame.sp;
    const auto regs = Nonvolatile(context);
    for (std::size_t i = 0; i != regs.size(); ++i)
        regs[i]->u64 = frame.r22_through_r31[i];
}

void SeedFree(GuestMemory& memory, GuestAddress address,
    std::uint16_t units, std::uint8_t flags)
{
    const GuestAddress head = Heap + (std::uint32_t{units} + 48u) * 8u;
    const GuestAddress link = address + 8u;
    memory.WriteU16(address, units);
    memory.WriteU8(address + 5u, flags);
    memory.WriteU32(head, link);
    memory.WriteU32(head + 4u, link);
    memory.WriteU32(link, head);
    memory.WriteU32(link + 4u, head);
    const GuestAddress word = Heap + ((units >> 5u) + 88u) * 4u;
    memory.WriteU32(word, memory.ReadU32(word) | (1u << (units & 31u)));
}

void Seed(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xBD, 0x400000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(Heap + 28u, 100u);
    memory.WriteU32(Heap + 48u, 100u);
    memory.WriteU32(Heap + 96u, Segment);
    memory.WriteU32(Segment + 64u, Block);
    memory.WriteU32(Segment + 56u, 0);
    for (std::uint32_t units = 2; units <= 10; ++units)
    {
        const GuestAddress head = Heap + (units + 48u) * 8u;
        memory.WriteU32(head, head);
        memory.WriteU32(head + 4u, head);
    }
    if (mode == Mode::MergeCompare)
    {
        const GuestAddress head = Heap + (99u + 48u) * 8u;
        memory.WriteU32(head, head);
        memory.WriteU32(head + 4u, head);
        memory.WriteU16(0x22000u, 3u);
    }
    memory.WriteU32(Heap + 384u, Heap + 384u);
    memory.WriteU32(Heap + 388u, Heap + 384u);
    const std::uint16_t old_units = mode == Mode::ShrinkSplit ? 8 : 4;
    memory.WriteU16(Block, old_units);
    memory.WriteU8(Block + 4u, 0);
    memory.WriteU8(Block + 5u,
        mode == Mode::SegmentMissing ? 0x11u :
        mode == Mode::NativeAlias ? 3u : 1u);
    memory.WriteU8(Block + 6u, 0);
    const GuestAddress adjacent = Block + (std::uint32_t{old_units} << 4u);
    if (mode == Mode::NeighborBusy || mode == Mode::Limit)
        memory.WriteU8(adjacent + 5u, 1);
    else if (mode != Mode::SegmentMissing)
    {
        const std::uint16_t free_units = mode == Mode::ShrinkSplit ? 2 : 4;
        SeedFree(memory, adjacent, free_units,
            mode == Mode::NativeAlias ? 4u : 0u);
        const GuestAddress following = adjacent + (std::uint32_t{free_units} << 4u);
        if (mode == Mode::MergeFollowing || mode == Mode::MergeCompare)
            SeedFree(memory, following, 2,
                mode == Mode::MergeCompare ? 4u : 0u);
        else
            memory.WriteU8(following + 5u, 1);
    }
}

bool Check(const Case& item)
{
    Window original, recovered;
    Seed(original, item.mode); Seed(recovered, item.mode);
    Services expected(original, item.mode), actual(recovered, item.mode);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0xAABBCCDD87654321ull;
    for (std::size_t i = 0; i != 10; ++i)
        Nonvolatile(context)[i]->u64 = 0x5566778800000000ull + i;
    context.r3.u64 = 0x9988776600010000ull;
    context.r4.u64 = item.mode == Mode::GrowExact ||
        item.mode == Mode::NativeAlias ? 8u : 0u;
    context.r5.u64 = 0x8877665500020000ull;
    context.r6.u64 = 64u;
    context.r7.u64 = 5u;
    if (item.mode == Mode::Limit) context.r7.u64 = 101u;
    if (item.mode == Mode::NeighborShort) context.r7.u64 = 9u;
    if (item.mode == Mode::GrowExact || item.mode == Mode::NativeAlias ||
        item.mode == Mode::SegmentMissing) context.r7.u64 = 8u;
    if (item.mode == Mode::GrowExact || item.mode == Mode::NativeAlias)
        context.r6.u64 = 96u;
    const auto original_inputs = std::array<std::uint64_t, 5>{
        context.r3.u64, context.r4.u64, context.r5.u64,
        context.r6.u64, context.r7.u64};
    auto frame = Frame(context);
    active = &expected;
    __imp__sub_827CBCA8(context, original.bytes);
    std::uint64_t result = 0xDEADBEEFu;
    if (!family::Apply(0x827CBCA8u, actual.memory, actual, actual, actual,
            original_inputs[0], original_inputs[1], original_inputs[2],
            original_inputs[3], original_inputs[4], Stack, frame, result))
        throw std::runtime_error("heap resize entry missing");
    const auto observed = Frame(context);
    const bool matched = result == item.expected &&
        result == context.r3.u64 && frame.lr == observed.lr &&
        frame.sp == observed.sp &&
        frame.r22_through_r31 == observed.r22_through_r31 &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x400000u) == 0;
    if (!matched)
        std::printf("FAIL heap-block mode=%u r3=%llX/%llX lr=%llX/%llX sp=%llX/%llX events=%zu/%zu\n",
            static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(frame.sp),
            static_cast<unsigned long long>(observed.sp),
            expected.events.size(), actual.events.size());
    return matched;
}
} // namespace

void OriginalExtend(PPCContext& context, std::uint8_t*)
{
    context.r3.u64 = ExtendHeapSegment(active->memory, *active,
        context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
        context.r1.u32 - 160u);
}
void OriginalCoalesce(PPCContext& context, std::uint8_t*)
{
    context.r3.u64 = CoalesceFreeBlocks(active->memory, *active,
        context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32);
}
void OriginalInsert(PPCContext& context, std::uint8_t*)
{ InsertFreeBlocks(active->memory, context.r3.u32, context.r4.u32, context.r5.u32); }
void OriginalFill(PPCContext& context, std::uint8_t*)
{
    context.r3.u64 = FillGuestMemory(active->memory,
        context.r3.u32, context.r4.u32, context.r5.u32);
}
void OriginalCompare(PPCContext& context, std::uint8_t*)
{
    auto frame = Frame(context);
    context.r3.u64 = active->CompareMemoryUlong(active->memory,
        context.r3.u32, context.r4.u32, context.r5.u32,
        context.r1.u64, frame);
    ToPpc(context, frame);
}

int main()
{
    try
    {
        for (const auto& item : Cases)
            if (!Check(item)) return 1;
        Window window;
        Services services(window, Mode::Limit);
        family::FrameRegisters frame{1, 2, {}};
        std::uint64_t result = 3;
        if (family::Apply(0xFFFFFFFFu, services.memory, services, services,
                services, 4, 5, 6, 7, 8, Stack, frame, result) ||
            frame.lr != 1 || frame.sp != 2 || result != 3)
            throw std::runtime_error("unknown address changed state");
        std::printf("PASS heap-block-resize %zu original PPC cases + unknown\n",
            std::size(Cases));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL heap-block-resize: %s\n", error.what());
        return 1;
    }
}
