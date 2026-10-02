// Appended after verbatim generated PPC bodies for 827B7408, 827B72E8 and
// 823CDCA8. Only their underlying allocation, free and notification calls
// are replaced by deterministic services.
#include "lo_semantics/query_lifecycle.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::size_t MemorySize = 0x40000;
constexpr GuestAddress ScratchStart = 0x3e000, StackTop = 0x3f000;
constexpr GuestAddress Device = 0x1000, Query = 0x2000;
constexpr GuestAddress EmbeddedPool = Device + 0x5424, NewPool = 0x9000;
constexpr GuestAddress SparePoolA = 0x11000, SparePoolB = 0x13000;
constexpr GuestAddress Data = 0xc000, ChangedData = 0xd000;

void Require(bool valid, const char* reason) {
    if (!valid) throw std::runtime_error(reason);
}

struct Scenario {
    std::uint32_t seed;
    std::array<std::uint8_t, 8> embedded_bitmap{};
    std::array<std::uint8_t, 8> new_bitmap{};
    GuestAddress query_result = Query;
    GuestAddress pool_result = NewPool;
    GuestAddress data_result = Data;
    GuestAddress new_next = 0;
    bool mutate_during_unlink = false;
};

struct Event {
    char kind;
    std::uint32_t a, b, c, result;
    bool operator==(const Event&) const = default;
};

struct Run {
    std::uint8_t* bytes;
    const Scenario* scenario;
    unsigned allocations = 0;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;

    std::uint32_t Read32(GuestAddress at) const {
        return (std::uint32_t(bytes[at]) << 24) | (std::uint32_t(bytes[at + 1]) << 16) |
               (std::uint32_t(bytes[at + 2]) << 8) | bytes[at + 3];
    }
    void Write32(GuestAddress at, std::uint32_t value) {
        bytes[at] = std::uint8_t(value >> 24);
        bytes[at + 1] = std::uint8_t(value >> 16);
        bytes[at + 2] = std::uint8_t(value >> 8);
        bytes[at + 3] = std::uint8_t(value);
    }
    void Snapshot() { snapshots.emplace_back(bytes, bytes + ScratchStart); }

    GuestAddress Allocate(std::uint32_t size, std::uint32_t flags) {
        constexpr std::array<std::uint32_t, 3> sizes{0x9c, 0x1780, 0x1000};
        constexpr std::array<std::uint32_t, 3> expected_flags{0x64800000, 0x64800000, 0xbc800000};
        const std::array<GuestAddress, 3> results{scenario->query_result,
                                                   scenario->pool_result, scenario->data_result};
        Require(allocations < sizes.size(), "unexpected allocation count");
        Require(size == sizes[allocations] && flags == expected_flags[allocations],
                "unexpected allocation arguments");
        Snapshot();
        const GuestAddress result = results[allocations++];
        events.push_back({'A', size, flags, 0, result});
        Snapshot();
        return result;
    }

    std::uint32_t Free(GuestAddress at, std::uint32_t flags) {
        Require(at != 0 && at + 0x30 < ScratchStart, "unexpected free address");
        Snapshot();
        bytes[at + 0x30] ^= 0x5a;
        if (scenario->mutate_during_unlink && at == ChangedData)
            Write32(NewPool + 12, SparePoolB);
        events.push_back({'F', at, flags, 0, 0x1234});
        Snapshot();
        return 0x1234;
    }

    void Notify(GuestAddress begin, GuestAddress end, std::uint32_t flags) {
        Snapshot();
        bytes[Device + 0x3000] ^= 0x80;
        if (scenario->mutate_during_unlink) {
            Write32(NewPool + 8, ChangedData);
            Write32(NewPool + 12, SparePoolA);
        }
        events.push_back({'N', begin, end, flags, 0});
        Snapshot();
    }
};

Run* active = nullptr;
struct Services final : QueryPoolServices {
    Run& run;
    explicit Services(Run& value) : run(value) {}
    GuestAddress Allocate(std::uint32_t size, std::uint32_t flags) override {
        return run.Allocate(size, flags);
    }
    void Free(GuestAddress at, std::uint32_t flags) override { run.Free(at, flags); }
    void NotifyRange(GuestAddress begin, GuestAddress end, std::uint32_t flags) override {
        run.Notify(begin, end, flags);
    }
};

void Setup(Run& run) {
    const Scenario& scenario = *run.scenario;
    std::memset(run.bytes + EmbeddedPool, 0, 16);
    std::copy(scenario.embedded_bitmap.begin(), scenario.embedded_bitmap.end(),
              run.bytes + EmbeddedPool);
    run.Write32(EmbeddedPool + 8, Data);
    std::copy(scenario.new_bitmap.begin(), scenario.new_bitmap.end(), run.bytes + NewPool);
    run.Write32(NewPool + 12, scenario.new_next);
    std::memset(run.bytes + SparePoolA, 0, 16);
    std::memset(run.bytes + SparePoolB, 0, 16);
    run.bytes[Query + 0x11] = 0x5a;
    run.bytes[Query + 0x14] = 0xa7;
}

struct Outcome {
    GuestAddress created = 0;
    std::uint32_t released = 0;
    std::vector<std::uint8_t> before_release;
};

Outcome OracleCall(Run& run) {
    PPCContext ctx{};
    ctx.r1.u64 = StackTop;
    ctx.lr = 0x82345678;
    ctx.r3.u64 = Device;
    constexpr std::array<std::uint64_t, 7> saved{
        0x1122334455667788ull, 0x2233445566778899ull, 0x33445566778899aaull,
        0x445566778899aabbull, 0x5566778899aabbccull, 0x66778899aabbccddull,
        0x778899aabbccddeeull};
    ctx.r25.u64 = saved[0]; ctx.r26.u64 = saved[1]; ctx.r27.u64 = saved[2];
    ctx.r28.u64 = saved[3]; ctx.r29.u64 = saved[4]; ctx.r30.u64 = saved[5];
    ctx.r31.u64 = saved[6];

    active = &run;
    oracle_CreateType9Query(ctx, run.bytes);
    Outcome result;
    result.created = ctx.r3.u32;
    result.before_release.assign(run.bytes, run.bytes + ScratchStart);
    if (result.created != 0) {
        ctx.r3.u64 = result.created;
        sub_823CDCA8(ctx, run.bytes);
        result.released = ctx.r3.u32;
    }
    active = nullptr;

    Require(ctx.r1.u64 == StackTop && ctx.lr == 0x82345678,
            "PPC stack pointer and link register restored");
    Require(ctx.r25.u64 == saved[0] && ctx.r26.u64 == saved[1] &&
            ctx.r27.u64 == saved[2] && ctx.r28.u64 == saved[3] &&
            ctx.r29.u64 == saved[4] && ctx.r30.u64 == saved[5] &&
            ctx.r31.u64 == saved[6], "PPC nonvolatile registers restored");
    return result;
}

Outcome SemanticCall(Run& run) {
    GuestMemory memory(0, {run.bytes, MemorySize});
    Services pool_services(run);
    PooledQueryServices services(memory, pool_services);
    Outcome result;
    result.created = CreateType9Query(memory, services, Device);
    result.before_release.assign(run.bytes, run.bytes + ScratchStart);
    if (result.created != 0)
        result.released = ReleaseQuery(memory, pool_services, result.created);
    return result;
}

void Compare(const Scenario& scenario, unsigned ordinal) {
    auto* oracle_bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    auto* semantic_bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(oracle_bytes && semantic_bytes, "allocate independent aligned guest windows");
    try {
        std::mt19937 random(scenario.seed);
        for (std::size_t i = 0; i < MemorySize; ++i)
            oracle_bytes[i] = std::uint8_t(random());
        Run oracle{oracle_bytes, &scenario}, semantic{semantic_bytes, &scenario};
        Setup(oracle);
        std::memcpy(semantic_bytes, oracle_bytes, MemorySize);
        const Outcome expected = OracleCall(oracle);
        const Outcome actual = SemanticCall(semantic);
        if (expected.created != actual.created || expected.released != actual.released ||
            expected.before_release != actual.before_release ||
            oracle.events != semantic.events || oracle.snapshots != semantic.snapshots ||
            !std::equal(oracle_bytes, oracle_bytes + ScratchStart, semantic_bytes)) {
            std::fprintf(stderr,
                "FAIL lifecycle case=%u seed=%08x created=%08x/%08x released=%08x/%08x events=%zu/%zu snapshots=%zu/%zu\n",
                ordinal, scenario.seed, expected.created, actual.created,
                expected.released, actual.released, oracle.events.size(),
                semantic.events.size(), oracle.snapshots.size(), semantic.snapshots.size());
            throw std::runtime_error("original PPC / recovered lifecycle mismatch");
        }
        Require(expected.created == scenario.query_result ||
                (scenario.query_result != 0 && (scenario.pool_result == 0 ||
                 scenario.data_result == 0) && expected.created == 0),
                "unexpected creation result");
    } catch (...) {
        active = nullptr;
        VirtualFree(oracle_bytes, 0, MEM_RELEASE);
        VirtualFree(semantic_bytes, 0, MEM_RELEASE);
        throw;
    }
    VirtualFree(oracle_bytes, 0, MEM_RELEASE);
    VirtualFree(semantic_bytes, 0, MEM_RELEASE);
}

Scenario Embedded(std::uint32_t seed, std::array<std::uint8_t, 8> bitmap) {
    Scenario value{seed};
    value.embedded_bitmap = bitmap;
    return value;
}

Scenario Expanded(std::uint32_t seed, std::array<std::uint8_t, 8> bitmap,
                  GuestAddress pool = NewPool, GuestAddress data = Data) {
    Scenario value{seed};
    value.embedded_bitmap.fill(0xff);
    value.new_bitmap = bitmap;
    value.pool_result = pool;
    value.data_result = data;
    return value;
}
} // namespace

PPC_FUNC(sub_827C9D88) { ctx.r3.u64 = active->Allocate(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_827C9DB0) { ctx.r3.u64 = active->Free(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_823EA178) { active->Notify(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); }

int main() {
    try {
        unsigned tested = 0;
        auto test = [&](const Scenario& scenario) { Compare(scenario, tested++); };
        test(Embedded(1, {}));
        test(Embedded(2, {0x81, 0x7e, 0, 0, 0, 0, 0, 0}));
        test(Embedded(3, {0xff, 0x7f, 0, 0, 0, 0, 0, 0}));
        test(Embedded(4, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}));
        test(Expanded(5, {}));
        test(Expanded(6, {0x7e, 0x80, 0x10, 0, 0, 0, 0, 0}));
        auto changed = Expanded(7, {});
        changed.mutate_during_unlink = true;
        test(changed);
        auto failed_query = Embedded(8, {});
        failed_query.query_result = 0;
        test(failed_query);
        test(Expanded(9, {}, 0));
        test(Expanded(10, {}, NewPool, 0));

        constexpr std::uint32_t Seed = 0x827b7408;
        std::mt19937 random(Seed);
        for (unsigned i = 0; i < 64; ++i) {
            std::array<std::uint8_t, 8> bitmap{};
            for (auto& byte : bitmap) byte = std::uint8_t(random());
            // Keep a free slot while retaining deterministic nonzero history.
            bitmap[random() % bitmap.size()] &= 0xfe;
            test(Embedded(random(), bitmap));
        }
        std::printf("PASS lifecycle %u (10 fixed, 64 seeded random; seed=%08x)\n", tested, Seed);
        std::puts("PASS original generated PPC create->initialize->release; return, callback trace/snapshots, pre-release and final guest bytes");
        std::puts("LIMIT synthetic lower services, bounded low guest memory, no concurrency or runtime scene; cache effects not modeled");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
