// Appended after pinned original PPC bodies and the actual ABI helpers.
#include "lo_semantics/manager_resize.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::uint64_t Space = std::uint64_t{1} << 32;
constexpr std::size_t LowCommit = 0x40000, LowCompare = 0x3d000;
constexpr GuestAddress StackTop = 0x3f000, Frame = StackTop - 144;
constexpr GuestAddress Manager = 0x10000, VTable = 0x18000;
constexpr GuestAddress List = 0x20000, Node = Manager + 2524u + 20u * 100u, OtherNode = 0x21100;
constexpr GuestAddress OldStorage = 0x25000, NewStorage = 0x28000;
constexpr GuestAddress AllocateMethod = 0x8234567b, FreeMethod = 0x8234569d;
constexpr std::uint64_t CallerLR = 0x81234567;
void Require(bool value, const char* reason) {
    if (!value) throw std::runtime_error(reason);
}
enum class Kind { Resize, Find };
enum class Path { AllocateOnly, FreeOnly, NodeSame, NodeDifferent,
                  NodeTooSmall, SentinelKeep, SentinelShrink,
                  SentinelTooSmall };
struct Case {
    Kind kind = Kind::Resize;
    Path path = Path::NodeDifferent;
    std::uint32_t seed = 0;
    std::uint64_t manager_reg = Manager;
    std::uint64_t old_reg = OldStorage;
    std::uint64_t new_reg = 48;
    std::uint64_t flags_reg = 8;
    std::uint32_t mode = 0, group = 0, selected = 5;
    bool mutate_node = false;
    bool null_result = false;
};
struct Event {
    char kind;
    GuestAddress method;
    std::array<std::uint64_t, 4> args;
    bool operator==(const Event&) const = default;
};
struct Image {
    std::uint8_t* bytes;
    Image() {
        bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
        Require(bytes != nullptr, "reserve sparse 4GiB guest space");
        Require(VirtualAlloc(bytes, LowCommit, MEM_COMMIT, PAGE_READWRITE) == bytes,
                "commit low guest space");
    }
    ~Image() { VirtualFree(bytes, 0, MEM_RELEASE); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
};
struct Run {
    std::uint8_t* bytes;
    Case c;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;
    std::uint32_t Read32(GuestAddress address) const {
        return (std::uint32_t(bytes[address]) << 24) |
               (std::uint32_t(bytes[address + 1]) << 16) |
               (std::uint32_t(bytes[address + 2]) << 8) | bytes[address + 3];
    }
    void Write32(GuestAddress address, std::uint32_t value) {
        bytes[address] = std::uint8_t(value >> 24);
        bytes[address + 1] = std::uint8_t(value >> 16);
        bytes[address + 2] = std::uint8_t(value >> 8);
        bytes[address + 3] = std::uint8_t(value);
    }
    void Snapshot() {
        snapshots.emplace_back(bytes, bytes + LowCompare);
        snapshots.back().insert(snapshots.back().end(), bytes + Frame - 8, bytes + Frame);
    }
    std::uint64_t Allocate(GuestAddress method, std::uint64_t manager,
                           std::uint64_t size, std::uint64_t flags) {
        Snapshot();
        events.push_back({'A', method, {manager, size, flags, 0}});
        Require(method == (AllocateMethod & ~3u), "allocate vtable target");
        if (c.mutate_node) {
            Write32(List + 12, OtherNode);
            Write32(OtherNode + 16, 96);
            Write32(List, 72);
        }
        bytes[0x500] ^= 0x5a;
        Snapshot();
        return c.null_result ? 0 : (0x1234567800000000ull | NewStorage);
    }
    void Free(GuestAddress method, std::uint64_t manager,
              std::uint64_t address) {
        Snapshot();
        events.push_back({'F', method, {manager, address, 0, 0}});
        Require(method == (FreeMethod & ~3u), "free vtable target");
        bytes[0x501] ^= 0xa5;
        Snapshot();
    }
};
Run* active = nullptr;
struct Services final : ManagerResizeServices {
    Run& run;
    explicit Services(Run& value) : run(value) {}
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
                                  std::uint64_t bytes, std::uint64_t flags) override {
        return run.Allocate(method, manager, bytes, flags);
    }
    void FreeStorage(GuestAddress method, std::uint64_t manager,
                     std::uint64_t address) override {
        run.Free(method, manager, address);
    }
};
void Setup(Run& run) {
    run.Write32(Manager, VTable);
    run.Write32(VTable + 4, AllocateMethod);
    run.Write32(VTable + 12, FreeMethod);
    if (run.c.kind == Kind::Find) {
        const std::uint32_t requested = static_cast<std::uint32_t>(run.c.new_reg);
        const GuestAddress offset = (requested + 878u) * 4u;
        if (offset < LowCommit - Manager)
            run.Write32(Manager + offset, run.c.selected);
        return;
    }
    const auto old = static_cast<std::uint32_t>(run.c.old_reg);
    const std::uint32_t high = std::rotl(old, 5) & 0x1fu;
    const std::uint32_t middle = std::rotl(old, 21) & 0xffe0u;
    run.Write32(Manager + (high + 846u) * 4u, List - middle);
    const GuestAddress sentinel = Manager + 3364u;
    run.Write32(List + 12, run.c.path == Path::SentinelKeep ||
                              run.c.path == Path::SentinelShrink ||
                              run.c.path == Path::SentinelTooSmall
                              ? sentinel : Node);
    run.Write32(List, 64);
    run.Write32(List + 4, 64);
    run.Write32(Node, run.c.mode);
    run.Write32(Node + 4, run.c.group);
    run.Write32(Node + 16, run.c.path == Path::NodeTooSmall ? 16 : 64);
    run.Write32(OtherNode + 16, 88);
    if (run.c.path == Path::NodeSame) {
        const std::uint32_t requested = static_cast<std::uint32_t>(run.c.new_reg);
        const GuestAddress offset = (requested + 878u) * 4u;
        // Mode zero uses manager + selected*20 + 2524.
        const std::uint32_t selected = (Node - Manager - 2524u) / 20u;
        Require(Manager + selected * 20u + 2524u == Node,
                "chosen node must be a helper-reachable address");
        run.Write32(Manager + offset, selected);
    }
}
void Compare(const Case& scenario, unsigned ordinal) {
    Image original_image, semantic_image;
    std::mt19937 random(scenario.seed);
    for (std::size_t i = 0; i < LowCommit; ++i)
        original_image.bytes[i] = std::uint8_t(random());
    std::memcpy(semantic_image.bytes, original_image.bytes, LowCommit);
    Run original{original_image.bytes, scenario}, semantic{semantic_image.bytes, scenario};
    Setup(original); Setup(semantic);
    PPCContext ctx{};
    ctx.r1.u64 = StackTop; ctx.lr = CallerLR;
    ctx.r3.u64 = scenario.manager_reg;
    ctx.r4.u64 = scenario.kind == Kind::Find ? scenario.new_reg : scenario.old_reg;
    ctx.r5.u64 = scenario.kind == Kind::Find ? scenario.mode : scenario.new_reg;
    ctx.r6.u64 = scenario.kind == Kind::Find ? scenario.group : scenario.flags_reg;
    const std::array<std::uint64_t, 6> saved{
        0x1122334455667788ull, 0x2233445566778899ull,
        0x33445566778899aaull, 0x445566778899aabbull,
        0x5566778899aabbccull, 0x66778899aabbccddull};
    ctx.r26.u64 = saved[0]; ctx.r27.u64 = saved[1]; ctx.r28.u64 = saved[2];
    ctx.r29.u64 = saved[3]; ctx.r30.u64 = saved[4]; ctx.r31.u64 = saved[5];
    active = &original;
    if (scenario.kind == Kind::Find)
        oracle_FindPrimaryResizeNode(ctx, original_image.bytes);
    else
        oracle_ResizePrimaryManagerStorage(ctx, original_image.bytes);
    active = nullptr;
    GuestMemory memory(0, {semantic_image.bytes, static_cast<std::size_t>(Space)});
    Services services(semantic);
    const std::uint64_t result = scenario.kind == Kind::Find
        ? FindPrimaryResizeNode(memory, scenario.manager_reg, scenario.new_reg,
                                scenario.mode, scenario.group)
        : ResizePrimaryManagerStorage(memory, services, scenario.manager_reg,
                                      scenario.old_reg, scenario.new_reg,
                                      scenario.flags_reg, Frame);
    if (ctx.r3.u64 != result || ctx.r1.u64 != StackTop || ctx.lr != CallerLR ||
        ctx.r26.u64 != saved[0] || ctx.r27.u64 != saved[1] || ctx.r28.u64 != saved[2] ||
        ctx.r29.u64 != saved[3] || ctx.r30.u64 != saved[4] || ctx.r31.u64 != saved[5] ||
        original.events != semantic.events || original.snapshots != semantic.snapshots) {
        std::fprintf(stderr, "case=%u kind=%u path=%u return=%016llx/%016llx events=%zu/%zu snapshots=%zu/%zu\n",
                     ordinal, unsigned(scenario.kind), unsigned(scenario.path),
                     static_cast<unsigned long long>(ctx.r3.u64),
                     static_cast<unsigned long long>(result),
                     original.events.size(), semantic.events.size(),
                     original.snapshots.size(), semantic.snapshots.size());
        throw std::runtime_error("register, callback or boundary snapshot mismatch");
    }
    for (std::size_t i = 0; i < LowCommit; ++i) {
        if (scenario.kind == Kind::Resize && i >= Frame && i < StackTop + 128u)
            continue; // Generic save/restore and stwu backchain.
        if (original_image.bytes[i] != semantic_image.bytes[i]) {
            std::fprintf(stderr, "case=%u first byte=%zx original=%02x semantic=%02x\n",
                         ordinal, i, original_image.bytes[i], semantic_image.bytes[i]);
            throw std::runtime_error("guest memory mismatch");
        }
    }
}
} // namespace

void oracle_Indirect(PPCContext& ctx, std::uint8_t*, std::uint32_t method) {
    if (method == (AllocateMethod & ~3u))
        ctx.r3.u64 = active->Allocate(method, ctx.r3.u64, ctx.r4.u64, ctx.r5.u64);
    else if (method == (FreeMethod & ~3u)) {
        active->Free(method, ctx.r3.u64, ctx.r4.u64);
        ctx.r3.u64 = 0xfedcba9876543210ull; // Caller ignores free's residual.
    } else
        throw std::runtime_error("unexpected indirect method");
}
PPC_FUNC(sub_822A0738) { oracle_FindPrimaryResizeNode(ctx, base); }
PPC_FUNC(sub_82B7A0B0) { oracle_CopyGuestMemory(ctx, base); }

int main() {
    try {
        unsigned resize = 0, find = 0;
        auto test = [&](Case c) {
            Compare(c, c.kind == Kind::Resize ? resize++ : find++);
        };
        for (auto path : {Path::AllocateOnly, Path::FreeOnly, Path::NodeSame,
                          Path::NodeDifferent, Path::NodeTooSmall,
                          Path::SentinelKeep, Path::SentinelShrink,
                          Path::SentinelTooSmall}) {
            Case c; c.path = path; c.seed = 0x595000u + resize;
            if (path == Path::AllocateOnly) c.old_reg = 0;
            if (path == Path::FreeOnly) c.new_reg = 0;
            if (path == Path::NodeSame) {
                c.mode = 0; c.group = 0;
            }
            if (path == Path::SentinelKeep) c.new_reg = 56;
            if (path == Path::SentinelShrink) c.new_reg = 16;
            if (path == Path::SentinelTooSmall) c.new_reg = 96;
            test(c);
            if (path == Path::NodeDifferent || path == Path::SentinelShrink) {
                c.seed += 100; c.mutate_node = true; test(c);
            }
        }
        for (unsigned i = 0; i < 18; ++i) {
            Case c; c.seed = 0x595100u + i;
            c.path = i % 2 ? Path::NodeDifferent : Path::SentinelShrink;
            c.new_reg = i % 2 ? 32 + i : 8 + i;
            c.flags_reg = 0xa5a5000000000000ull | i;
            c.manager_reg = (std::uint64_t(i + 1) << 32) | Manager;
            c.old_reg = (std::uint64_t(i + 2) << 32) | OldStorage;
            c.mutate_node = (i % 3) == 0;
            c.null_result = (i % 7) == 0;
            test(c);
        }
        for (unsigned i = 0; i < 4; ++i) {
            Case c; c.seed = 0x595300u + i;
            c.path = i & 1u ? Path::SentinelShrink : Path::NodeDifferent;
            c.new_reg = (std::uint64_t(0x80000001u + i) << 32) |
                        (i & 1u ? 16u : 48u);
            c.manager_reg = 0x1020304000000000ull | Manager;
            c.old_reg = 0x5060708000000000ull | OldStorage;
            c.flags_reg = 0x90a0b0c000000008ull;
            test(c);
        }
        for (std::uint32_t requested : {0u, 1u, 64u, 32768u, 32769u,
                                        0xffffffffu, 0xfffffc92u}) {
            for (std::uint32_t mode : {0u, 1u}) {
                Case c; c.kind = Kind::Find; c.new_reg = requested;
                c.mode = mode; c.group = 3; c.selected = 7;
                c.manager_reg = 0xabcdef1200000000ull | Manager;
                c.seed = 0xa073800u + find;
                test(c);
                if (requested == 64u && mode == 1u) {
                    c.new_reg = 0x1234567800000040ull;
                    c.seed += 1000;
                    test(c);
                }
            }
        }
        std::printf("PASS 82295950 %u\nPASS 822A0738 %u\n", resize, find);
        std::puts("LIMIT generated PPC; virtual allocate/free synthetic; 4GiB sparse ordinary guest memory; generic ABI spill excluded");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
