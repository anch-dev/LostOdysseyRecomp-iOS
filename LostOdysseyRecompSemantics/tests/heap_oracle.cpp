// Appended after verbatim original generated PPC functions and their ABI helpers.
#include "lo_semantics/heap.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::size_t MemorySize = 0x400000, ScratchStart = 0x3e0000;
constexpr GuestAddress StackTop = 0x3f0000, Heap = 0x10000, Units = 0x20000;
constexpr GuestAddress Descriptor = 0x50000, Descriptor2 = 0x51000;
constexpr GuestAddress Block = 0x100000, Existing = 0x280000, Existing2 = 0x290000;
constexpr GuestAddress SegmentEnd = 0x300000;

void Require(bool okay, const char* reason) {
    if (!okay) throw std::runtime_error(reason);
}
enum class Kind { Insert, Coalesce };
struct Case {
    Kind kind;
    std::uint32_t units;
    std::uint32_t already_free = 0;
    std::uint32_t prev_units = 0, next_units = 0;
    bool prev_free = false, next_free = false;
    bool bad_backlink = false, endpoint = false;
    std::uint32_t endpoint_units = 0;
    std::uint8_t flags = 0, prev_flags = 0, next_flags = 0;
    std::uint8_t segment_index = 0;
    std::uint16_t old_previous = 7;
    std::uint16_t existing_large = 0, existing_large2 = 0;
    std::uint16_t existing_small = 0;
    std::string expected_callbacks;
};
struct Event {
    GuestAddress source;
    std::uint32_t bytes, value, result;
    bool operator==(const Event&) const = default;
};
struct Run {
    uint8_t* memory;
    std::vector<Event> callbacks;
    std::vector<std::vector<uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress address) const {
        return (std::uint32_t(memory[address]) << 24) |
               (std::uint32_t(memory[address + 1]) << 16) |
               (std::uint32_t(memory[address + 2]) << 8) | memory[address + 3];
    }
    void Write16(GuestAddress address, std::uint16_t value) {
        memory[address] = std::uint8_t(value >> 8);
        memory[address + 1] = std::uint8_t(value);
    }
    void Write32(GuestAddress address, std::uint32_t value) {
        memory[address] = std::uint8_t(value >> 24);
        memory[address + 1] = std::uint8_t(value >> 16);
        memory[address + 2] = std::uint8_t(value >> 8);
        memory[address + 3] = std::uint8_t(value);
    }
    void Snapshot() { snapshots.emplace_back(memory, memory + ScratchStart); }
    std::uint32_t CompareMemoryUlong(GuestAddress source, std::uint32_t bytes,
                                     std::uint32_t value) {
        Require(value == 0xfeeefeee, "PPC debug fill pattern");
        Require(source >= Block - 0x100 && source + bytes < MemorySize,
                "PPC debug comparison address");
        Snapshot();
        callbacks.push_back({source, bytes, value, 0x5678});
        Snapshot();
        return 0x5678;
    }
};
Run* active = nullptr;
struct Services final : HeapServices {
    Run& run;
    explicit Services(Run& value) : run(value) {}
    std::uint32_t CompareMemoryUlong(GuestAddress source, std::uint32_t bytes,
                                     std::uint32_t value) override {
        return run.CompareMemoryUlong(source, bytes, value);
    }
};
GuestAddress Head(std::uint32_t size) { return Heap + (size + 48) * 8; }
GuestAddress Bitmap(std::uint32_t size) { return Heap + ((size >> 5) + 88) * 4; }

void LinkSingleton(Run& run, GuestAddress block, std::uint32_t size) {
    const auto head = Head(size);
    const auto node = block + 8;
    Require(run.Read32(head) == head, "fixture bucket must start empty");
    run.Write32(head, node); run.Write32(head + 4, node);
    run.Write32(node, head); run.Write32(node + 4, head);
    if (size < 128)
        run.Write32(Bitmap(size), run.Read32(Bitmap(size)) | (1u << (size & 31)));
    run.Write32(Heap + 48, run.Read32(Heap + 48) + size);
}
void Header(Run& run, GuestAddress block, std::uint16_t size,
            std::uint16_t previous, std::uint8_t flags) {
    run.Write16(block, size);
    run.Write16(block + 2, previous);
    run.memory[block + 4] = 0;
    run.memory[block + 5] = flags;
}
void InitializeCommon(Run& run) {
    std::memset(run.memory, 0xa5, MemorySize);
    for (unsigned size = 0; size < 128; ++size) {
        const auto head = Head(size);
        run.Write32(head, head);
        run.Write32(head + 4, head);
    }
    for (unsigned word = 0; word < 4; ++word)
        run.Write32(Heap + (88 + word) * 4, 0);
    run.Write32(Heap + 48, 0);
    run.Write32(Heap + (24 + 0) * 4, Descriptor);
    run.Write32(Heap + (24 + 1) * 4, Descriptor2);
    run.Write32(Descriptor + 44, SegmentEnd);
    run.Write32(Descriptor2 + 44, SegmentEnd);
    run.Write32(Descriptor + 64, Block);
    run.Write32(Descriptor2 + 64, Block);
    run.Write32(Units, 0);
}
void SeedLargeList(Run& run, std::uint16_t first, std::uint16_t second) {
    const auto head = Head(0);
    Header(run, Existing, first, 0, 0);
    const auto one = Existing + 8;
    run.Write32(head, one); run.Write32(one + 4, head);
    if (second != 0) {
        Header(run, Existing2, second, 0, 0);
        const auto two = Existing2 + 8;
        run.Write32(one, two); run.Write32(two + 4, one);
        run.Write32(two, head); run.Write32(head + 4, two);
    } else {
        run.Write32(one, head); run.Write32(head + 4, one);
    }
}
void SetupInsert(Run& run, const Case& scenario) {
    Header(run, Block, 0, scenario.old_previous, scenario.flags);
    run.memory[Block + 4] = scenario.segment_index;
    if (scenario.endpoint)
        run.Write32((scenario.segment_index ? Descriptor2 : Descriptor) + 44,
                    Block + ((scenario.endpoint_units ? scenario.endpoint_units : scenario.units) << 4));
    if (scenario.existing_large)
        SeedLargeList(run, scenario.existing_large, scenario.existing_large2);
    if (scenario.existing_small) {
        Header(run, Existing, scenario.existing_small, 0, 0);
        LinkSingleton(run, Existing, scenario.existing_small);
    }
}
void SetupCoalesce(Run& run, const Case& scenario) {
    run.Write32(Units, scenario.units);
    Header(run, Block, static_cast<std::uint16_t>(scenario.units),
           static_cast<std::uint16_t>(scenario.prev_units), scenario.flags);
    if (scenario.prev_units != 0) {
        const auto previous = Block - scenario.prev_units * 16;
        Header(run, previous, static_cast<std::uint16_t>(scenario.prev_units),
               0, scenario.prev_free ? scenario.prev_flags : std::uint8_t(1));
        if (scenario.prev_free) {
            LinkSingleton(run, previous, scenario.prev_units);
            if (scenario.bad_backlink)
                run.Write32(Head(scenario.prev_units), Head(scenario.prev_units));
        }
    }
    const auto next = Block + (scenario.units << 4);
    Header(run, next, static_cast<std::uint16_t>(scenario.next_units),
           static_cast<std::uint16_t>(scenario.units),
           scenario.next_free ? scenario.next_flags : std::uint8_t(1));
    if (scenario.next_free)
        LinkSingleton(run, next, scenario.next_units);
    if (scenario.already_free)
        LinkSingleton(run, Block, scenario.units);
    // A following allocated header gives both merge directions a bounded
    // next-block target when a successful merge writes its boundary tag.
    Header(run, next + scenario.next_units * 16, 1,
           static_cast<std::uint16_t>(scenario.next_units), 1);
}
PPCFunc* Oracle(Kind kind) {
    return kind == Kind::Insert ? oracle_InsertFreeBlocks : oracle_CoalesceFreeBlocks;
}
void Compare(const Case& scenario, unsigned ordinal) {
    auto* original_bytes = static_cast<uint8_t*>(VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE,
                                                              PAGE_READWRITE));
    auto* recovered_bytes = static_cast<uint8_t*>(VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE,
                                                               PAGE_READWRITE));
    Require(original_bytes && recovered_bytes, "allocate independent aligned guest windows");
    try {
        Run original{original_bytes}, recovered{recovered_bytes};
        InitializeCommon(original);
        if (scenario.kind == Kind::Insert) SetupInsert(original, scenario);
        else SetupCoalesce(original, scenario);
        std::memcpy(recovered_bytes, original_bytes, MemorySize);
        PPCContext ctx{};
        ctx.r1.u64 = StackTop; ctx.lr = 0x82345678;
        ctx.r3.u64 = Heap; ctx.r4.u64 = Block;
        ctx.r5.u64 = scenario.kind == Kind::Insert ? scenario.units : Units;
        ctx.r6.u64 = scenario.already_free;
        constexpr std::array<std::uint64_t, 7> saved = {
            0x1122334455667788ull, 0x2233445566778899ull, 0x33445566778899aaull,
            0x445566778899aabbull, 0x5566778899aabbccull, 0x66778899aabbccddull,
            0x778899aabbccddeeull};
        ctx.r25.u64 = saved[0]; ctx.r26.u64 = saved[1]; ctx.r27.u64 = saved[2];
        ctx.r28.u64 = saved[3]; ctx.r29.u64 = saved[4]; ctx.r30.u64 = saved[5]; ctx.r31.u64 = saved[6];
        active = &original;
        Oracle(scenario.kind)(ctx, original_bytes);
        active = nullptr;
        Require(ctx.r1.u64 == StackTop && ctx.lr == 0x82345678,
                "original PPC stack and LR restored");
        Require(ctx.r25.u64 == saved[0] && ctx.r26.u64 == saved[1] && ctx.r27.u64 == saved[2] &&
                ctx.r28.u64 == saved[3] && ctx.r29.u64 == saved[4] && ctx.r30.u64 == saved[5] &&
                ctx.r31.u64 == saved[6], "original PPC nonvolatile registers restored");
        Require(original.callbacks.size() == scenario.expected_callbacks.size(),
                "original PPC debug callback count");
        GuestMemory memory(0, {recovered_bytes, MemorySize});
        Services services(recovered);
        const auto result = scenario.kind == Kind::Insert ?
            (InsertFreeBlocks(memory, Heap, Block, scenario.units), Heap) :
            CoalesceFreeBlocks(memory, services, Heap, Block, Units, scenario.already_free);
        if ((scenario.kind == Kind::Coalesce && ctx.r3.u32 != result) ||
            original.callbacks != recovered.callbacks ||
            original.snapshots != recovered.snapshots ||
            !std::equal(original_bytes, original_bytes + ScratchStart, recovered_bytes)) {
            std::fprintf(stderr,
                "FAIL kind=%u case=%u units=%u prev=%u next=%u free=%u return=%08x/%08x callbacks=%zu/%zu\n",
                unsigned(scenario.kind), ordinal, scenario.units, scenario.prev_units,
                scenario.next_units, scenario.already_free, ctx.r3.u32, result,
                original.callbacks.size(), recovered.callbacks.size());
            throw std::runtime_error("PPC/recovered heap mismatch");
        }
    } catch (...) {
        active = nullptr;
        VirtualFree(original_bytes, 0, MEM_RELEASE);
        VirtualFree(recovered_bytes, 0, MEM_RELEASE);
        throw;
    }
    VirtualFree(original_bytes, 0, MEM_RELEASE);
    VirtualFree(recovered_bytes, 0, MEM_RELEASE);
}
Case Insert(std::uint32_t units) { return {Kind::Insert, units}; }
Case Merge(std::uint32_t units) { return {Kind::Coalesce, units}; }
} // namespace

PPC_FUNC(__imp__RtlCompareMemoryUlong) {
    ctx.r3.u64 = active->CompareMemoryUlong(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}

int main() {
    try {
        unsigned insert_count = 0, coalesce_count = 0;
        auto test = [&](const Case& scenario) {
            Compare(scenario, scenario.kind == Kind::Insert ? insert_count++ : coalesce_count++);
        };
        for (std::uint32_t units : {0u, 1u, 2u, 31u, 32u, 127u, 128u,
                                    0xefffu, 0xf000u, 0xf001u, 0xf002u})
            test(Insert(units));
        {
            auto value = Insert(150);
            value.existing_large = 130; value.existing_large2 = 200;
            test(value);
        }
        {
            auto value = Insert(150);
            value.existing_large = 150; // equal sizes insert before the existing node
            test(value);
        }
        {
            auto value = Insert(2);
            value.existing_small = 2;
            test(value);
        }
        {
            auto value = Insert(31);
            value.endpoint = true; value.segment_index = 1;
            test(value);
        }
        {
            auto value = Insert(2);
            value.flags = 0xf7;
            test(value);
        }
        {
            auto value = Insert(0xf002);
            value.endpoint = true; value.endpoint_units = 0xf000;
            test(value); // End after the first chunk, with two units still pending.
        }
        for (const auto existing : {130u, 200u}) {
            auto value = Insert(150);
            value.existing_large = std::uint16_t(existing);
            test(value); // Sorted large-list tail and head.
        }
        {
            auto value = Merge(4);
            value.flags = 0x10; // segment end skips next block
            test(value);
        }
        {
            auto value = Merge(4);
            value.prev_units = 2; value.prev_free = true; value.flags = 0x10;
            test(value);
        }
        {
            auto value = Merge(4);
            value.next_units = 3; value.next_free = true;
            test(value);
        }
        {
            auto value = Merge(4);
            value.prev_units = 2; value.prev_free = true;
            value.next_units = 3; value.next_free = true;
            test(value);
        }
        {
            auto value = Merge(4);
            value.prev_units = 2; value.prev_free = true;
            value.next_units = 3; value.next_free = true;
            value.already_free = 1;
            test(value);
        }
        {
            auto value = Merge(4);
            value.next_units = 3; value.next_free = true;
            value.already_free = 1; value.flags = 0x06; value.next_flags = 0x04;
            value.expected_callbacks = "CC";
            test(value);
        }
        {
            auto value = Merge(0xf000);
            value.prev_units = 1; value.prev_free = true;
            value.next_units = 2; value.next_free = true;
            test(value);
        }
        {
            auto value = Merge(4);
            value.prev_units = 2; value.prev_free = true; value.bad_backlink = true;
            value.flags = 0x10;
            test(value);
        }
        {
            auto value = Merge(4);
            value.next_units = 3; value.next_free = true; value.next_flags = 0x10;
            test(value); // Segment last-block pointer changes after a next merge.
        }
        {
            auto value = Merge(0xeffe);
            value.prev_units = 2; value.prev_free = true; value.flags = 0x10;
            test(value); // Merge exactly to the allowed 0xf000-unit limit.
        }
        {
            auto value = Merge(0xeffd);
            value.next_units = 3; value.next_free = true; value.next_flags = 0x10;
            test(value);
        }
        std::mt19937 random(0x823ae108);
        for (unsigned i = 0; i < 128; ++i) {
            auto value = Merge(16 + random() % 16);
            value.prev_units = 2 + random() % 5;
            value.next_units = 8 + random() % 5;
            value.prev_free = (i & 1) != 0;
            value.next_free = (i & 2) != 0;
            value.already_free = (i & 4) != 0;
            value.flags = (i & 8) ? 0x10 : 0;
            value.next_flags = (i & 16) ? 0x10 : 0;
            test(value); // Distinct small buckets and bounded valid neighbors.
        }
        std::printf("PASS 827CBA60 %u\n", insert_count);
        std::printf("PASS 823AE108 %u\n", coalesce_count);
        std::puts("LIMIT: bounded synthetic heap links; callback return opaque; no corruption recovery or concurrent allocator behavior");
        return 0;
    } catch (const std::exception& error) {
        active = nullptr;
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
