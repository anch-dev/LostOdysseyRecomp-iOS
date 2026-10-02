// Appended after verbatim, renamed generated PPC functions and ABI helpers.
// The four oracle_* entry points above are the differential reference.
#include "lo_semantics/allocation.h"
#include "lo_semantics/query_pool.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::size_t MemorySize = 0x40000;
constexpr GuestAddress ScratchStart = 0x3e000, StackTop = 0x3f000;
constexpr GuestAddress Device = 0x1000, Query = 0x2000;
constexpr GuestAddress EmbeddedPool = Device + 0x5424, ExtraPool = 0x8000;
constexpr GuestAddress NewPool = 0x9000, Data = 0xc000;
constexpr GuestAddress SecondPool = 0xa000, ReplacementData = 0xe000;
constexpr GuestAddress OwnerSlot = 0x2800, DataSlot = 0x2804;

enum class Kind { Initialize, Release, AllocateDispatch, FreeDispatch };
struct Run;
struct Case {
    Kind kind;
    std::uint32_t seed;
    std::uint32_t a = 0, b = 0, c = 0;
    std::array<GuestAddress, 2> allocations{};
    std::uint32_t callback_result = 0;
    bool grow_count_on_notify = false;
    bool notify_changes_data = false;
    bool data_free_changes_next = false;
    bool pool_free_reinserts_once = false;
    std::string expected_trace;
    std::function<void(Run&)> setup;
};
struct Event {
    char kind;
    std::uint32_t a, b, c, result;
    bool operator==(const Event&) const = default;
};
void Require(bool okay, const char* reason) {
    if (!okay) throw std::runtime_error(reason);
}
struct Run {
    uint8_t* bytes;
    const Case* scenario;
    unsigned allocation_index = 0;
    bool reinserted = false;
    std::vector<Event> events;
    std::vector<std::vector<uint8_t>> snapshots;

    std::uint32_t Read32(GuestAddress at) const {
        return (std::uint32_t(bytes[at]) << 24) | (std::uint32_t(bytes[at + 1]) << 16) |
               (std::uint32_t(bytes[at + 2]) << 8) | bytes[at + 3];
    }
    void Write32(GuestAddress at, std::uint32_t value) {
        bytes[at] = std::uint8_t(value >> 24); bytes[at + 1] = std::uint8_t(value >> 16);
        bytes[at + 2] = std::uint8_t(value >> 8); bytes[at + 3] = std::uint8_t(value);
    }
    void Snapshot() { snapshots.emplace_back(bytes, bytes + ScratchStart); }
    GuestAddress PoolAllocate(std::uint32_t size, std::uint32_t flags) {
        Require(allocation_index < scenario->allocations.size(), "unexpected pool allocation");
        Snapshot();
        const auto result = scenario->allocations[allocation_index++];
        events.push_back({'A', size, flags, 0, result});
        Snapshot();
        return result;
    }
    std::uint32_t PoolFree(GuestAddress at, std::uint32_t flags) {
        Require(at != 0 && at + 0x30 < ScratchStart, "unexpected pool free address");
        Snapshot();
        bytes[at + 0x30] = 0xee;
        if (scenario->data_free_changes_next &&
            (at == Data + 0x1000 || at == ReplacementData))
            Write32(ExtraPool + 12, SecondPool);
        if (scenario->pool_free_reinserts_once && at == ExtraPool && !reinserted) {
            Write32(EmbeddedPool + 12, ExtraPool);
            reinserted = true;
        }
        events.push_back({'F', at, flags, 0, 0x1234});
        Snapshot();
        return 0x1234;
    }
    void Notify(GuestAddress begin, GuestAddress end, std::uint32_t flags) {
        Snapshot();
        bytes[Device + 0x3000] ^= 0x80;
        if (scenario->notify_changes_data) Write32(ExtraPool + 8, ReplacementData);
        if (scenario->grow_count_on_notify) Write32(Query + 0x98, 2);
        events.push_back({'N', begin, end, flags, 0});
        Snapshot();
    }
    GuestAddress AllocateSpecial(GuestAddress allocator, std::uint32_t size) {
        Snapshot();
        bytes[0x400] ^= 0x20;
        events.push_back({'S', allocator, size, 0, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    GuestAddress AllocateGeneral(std::uint32_t size, std::uint32_t flags) {
        Snapshot();
        bytes[0x400] ^= 0x40;
        events.push_back({'G', size, flags, 0, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    std::uint32_t FreeSpecial(GuestAddress allocator, GuestAddress address) {
        Snapshot();
        bytes[0x400] ^= 0x10;
        events.push_back({'s', allocator, address, 0, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    std::uint32_t FreeGeneral(GuestAddress address, std::uint32_t flags) {
        Snapshot();
        bytes[0x400] ^= 0x08;
        events.push_back({'g', address, flags, 0, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
};
Run* active = nullptr;
struct Services final : QueryPoolServices, AllocationServices {
    Run& run;
    explicit Services(Run& value) : run(value) {}
    GuestAddress Allocate(std::uint32_t bytes, std::uint32_t flags) override {
        return run.PoolAllocate(bytes, flags);
    }
    void Free(GuestAddress address, std::uint32_t flags) override { run.PoolFree(address, flags); }
    void NotifyRange(GuestAddress begin, GuestAddress end, std::uint32_t flags) override {
        run.Notify(begin, end, flags);
    }
    GuestAddress AllocateSpecial(GuestAddress allocator, std::uint32_t bytes) override {
        return run.AllocateSpecial(allocator, bytes);
    }
    GuestAddress AllocateGeneral(std::uint32_t bytes, std::uint32_t flags) override {
        return run.AllocateGeneral(bytes, flags);
    }
    std::uint32_t FreeSpecial(GuestAddress allocator, GuestAddress address) override {
        return run.FreeSpecial(allocator, address);
    }
    std::uint32_t FreeGeneral(GuestAddress address, std::uint32_t flags) override {
        return run.FreeGeneral(address, flags);
    }
};

void InitializePool(Run& run, GuestAddress pool, GuestAddress data) {
    std::memset(run.bytes + pool, 0, 16);
    run.Write32(pool + 8, data);
}
Case InitCase(std::uint32_t seed, unsigned byte_index, unsigned bit_index,
              bool linked = false, bool new_pool = false, bool alias = false,
              GuestAddress first = NewPool, GuestAddress second = Data,
              bool nonzero_new = false) {
    Case value{Kind::Initialize, seed, Device, OwnerSlot, alias ? OwnerSlot : DataSlot};
    value.allocations = {first, second};
    value.expected_trace = !new_pool ? "" : first == 0 ? "A" : second == 0 ? "AAF" : "AA";
    value.setup = [=](Run& run) {
        InitializePool(run, EmbeddedPool, Data);
        if (linked) {
            std::memset(run.bytes + EmbeddedPool, 0xff, 8);
            run.Write32(EmbeddedPool + 12, ExtraPool);
            InitializePool(run, ExtraPool, Data);
        }
        if (new_pool) {
            std::memset(run.bytes + EmbeddedPool, 0xff, 8);
            run.Write32(EmbeddedPool + 12, 0);
            std::memset(run.bytes + NewPool, nonzero_new ? 0x7e : 0, 8);
        }
        GuestAddress chosen = linked ? ExtraPool : new_pool ? NewPool : EmbeddedPool;
        if (!new_pool || first != 0) {
            for (unsigned b = 0; b < byte_index && b < 8; ++b) run.bytes[chosen + b] = 0xff;
            if (byte_index < 8)
                run.bytes[chosen + byte_index] = std::uint8_t((1u << bit_index) - 1u);
            else std::memset(run.bytes + chosen, 0xff, 8);
        }
        run.Write32(OwnerSlot, 0xa5a5a5a5);
        if (!alias) run.Write32(DataSlot, 0x5a5a5a5a);
    };
    return value;
}

Case ReleaseCase(std::uint32_t seed, std::uint32_t refcount, std::uint32_t type,
                 bool extra = false, std::uint32_t count = 1,
                 std::uint32_t flags_word = 0, bool grow_count = false,
                 std::int32_t slot_delta = 0) {
    Case value{Kind::Release, seed, Query};
    value.grow_count_on_notify = grow_count;
    value.expected_trace = refcount != 1 ? "" :
        (type == 9 && count == 0) ? "F" :
        (type == 9 || type == 10) && extra ? "NFFF" : "F";
    value.setup = [=](Run& run) {
        InitializePool(run, EmbeddedPool, Data);
        InitializePool(run, ExtraPool, Data + 0x1000);
        InitializePool(run, SecondPool, Data + 0x2000);
        run.Write32(Query, Device); run.Write32(Query + 4, type);
        run.Write32(Query + 12, refcount); run.Write32(Query + 16, flags_word);
        run.Write32(Query + 0x98, count);
        run.Write32(Query + 0x58, extra ? ExtraPool : EmbeddedPool);
        run.Write32(Query + 0x1c, (extra ? Data + 0x1000 : Data) + std::uint32_t(slot_delta));
        run.Write32(Query + 0x5c, EmbeddedPool);
        run.Write32(Query + 0x20, Data + 64);
        run.bytes[extra ? ExtraPool : EmbeddedPool] = 1;
        if (count > 1 || grow_count) run.bytes[EmbeddedPool] |= 2;
        if (extra) run.Write32(EmbeddedPool + 12, ExtraPool);
        run.bytes[Device + 10943] = 0x85;
    };
    return value;
}

Case DispatchCase(Kind kind, std::uint32_t seed, std::uint32_t arg,
                  std::uint32_t flags, std::uint32_t result) {
    Case value{kind, seed, arg, flags};
    value.callback_result = result;
    value.expected_trace = kind == Kind::AllocateDispatch ?
        (flags == 0xa7820007 ? "S" : "G") : (flags == 0xa7820007 ? "s" : "g");
    return value;
}

std::uint32_t SemanticCall(const Case& scenario, Run& run) {
    GuestMemory memory(0, {run.bytes, MemorySize});
    Services services(run);
    switch (scenario.kind) {
    case Kind::Initialize:
        return std::uint32_t(InitializeQuerySlot(memory, services, scenario.a, scenario.b, scenario.c));
    case Kind::Release:
        return ReleaseQuery(memory, services, scenario.a);
    case Kind::AllocateDispatch:
        return AllocateDispatch(services, scenario.a, scenario.b);
    case Kind::FreeDispatch:
        return FreeDispatch(services, scenario.a, scenario.b);
    }
    throw std::runtime_error("unknown fixture kind");
}

PPCFunc* OracleFunction(Kind kind) {
    switch (kind) {
    case Kind::Initialize: return oracle_InitializeQuerySlot;
    case Kind::Release: return oracle_ReleaseQuery;
    case Kind::AllocateDispatch: return oracle_AllocateDispatch;
    case Kind::FreeDispatch: return oracle_FreeDispatch;
    }
    throw std::runtime_error("unknown oracle kind");
}

void Compare(const Case& scenario, unsigned ordinal) {
    auto* oracle_bytes = static_cast<uint8_t*>(VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    auto* recovered_bytes = static_cast<uint8_t*>(VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(oracle_bytes && recovered_bytes, "allocate independent aligned guest windows");
    try {
        std::mt19937 random(scenario.seed);
        for (std::size_t i = 0; i < MemorySize; ++i) oracle_bytes[i] = std::uint8_t(random());
        Run oracle{oracle_bytes, &scenario}, recovered{recovered_bytes, &scenario};
        if (scenario.setup) scenario.setup(oracle);
        std::memcpy(recovered_bytes, oracle_bytes, MemorySize);
        PPCContext ctx{};
        ctx.r1.u64 = StackTop; ctx.lr = 0x82345678;
        ctx.r3.u64 = scenario.a; ctx.r4.u64 = scenario.b; ctx.r5.u64 = scenario.c;
        constexpr std::array<std::uint64_t, 7> saved = {
            0x1122334455667788ull, 0x2233445566778899ull, 0x33445566778899aaull,
            0x445566778899aabbull, 0x5566778899aabbccull, 0x66778899aabbccddull,
            0x778899aabbccddeeull};
        ctx.r25.u64 = saved[0]; ctx.r26.u64 = saved[1]; ctx.r27.u64 = saved[2];
        ctx.r28.u64 = saved[3]; ctx.r29.u64 = saved[4]; ctx.r30.u64 = saved[5]; ctx.r31.u64 = saved[6];
        active = &oracle;
        OracleFunction(scenario.kind)(ctx, oracle_bytes);
        active = nullptr;
        std::string trace;
        for (const auto& event : oracle.events) trace += event.kind;
        Require(trace == scenario.expected_trace, "original PPC callback order");
        if (scenario.notify_changes_data) {
            const auto first_free = std::find_if(oracle.events.begin(), oracle.events.end(),
                                                 [](const Event& event) { return event.kind == 'F'; });
            Require(first_free != oracle.events.end() && first_free->a == ReplacementData,
                    "PPC reloads pool data pointer after notification");
        }
        if (scenario.data_free_changes_next)
            Require(oracle.Read32(EmbeddedPool + 12) == SecondPool,
                    "PPC reloads next pool after data free");
        Require(ctx.r1.u64 == StackTop && ctx.lr == 0x82345678, "PPC stack and LR restored");
        Require(ctx.r25.u64 == saved[0] && ctx.r26.u64 == saved[1] && ctx.r27.u64 == saved[2] &&
                ctx.r28.u64 == saved[3] && ctx.r29.u64 == saved[4] && ctx.r30.u64 == saved[5] &&
                ctx.r31.u64 == saved[6], "PPC nonvolatile registers restored");
        const auto semantic_result = SemanticCall(scenario, recovered);
        if (ctx.r3.u32 != semantic_result || oracle.events != recovered.events ||
            oracle.snapshots != recovered.snapshots ||
            !std::equal(oracle_bytes, oracle_bytes + ScratchStart, recovered_bytes)) {
            std::fprintf(stderr,
                "FAIL kind=%u case=%u seed=%08x args=%08x,%08x,%08x return=%08x/%08x events=%zu/%zu snapshots=%zu/%zu\n",
                unsigned(scenario.kind), ordinal, scenario.seed, scenario.a, scenario.b, scenario.c,
                ctx.r3.u32, semantic_result, oracle.events.size(), recovered.events.size(),
                oracle.snapshots.size(), recovered.snapshots.size());
            throw std::runtime_error("original PPC / recovered C++ mismatch");
        }
    } catch (...) {
        active = nullptr;
        VirtualFree(oracle_bytes, 0, MEM_RELEASE);
        VirtualFree(recovered_bytes, 0, MEM_RELEASE);
        throw;
    }
    VirtualFree(oracle_bytes, 0, MEM_RELEASE);
    VirtualFree(recovered_bytes, 0, MEM_RELEASE);
}
} // namespace

// Opaque dependencies called by the extracted original function bodies.
PPC_FUNC(sub_827C9D88) { ctx.r3.u64 = active->PoolAllocate(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_827C9DB0) { ctx.r3.u64 = active->PoolFree(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_823EA178) { active->Notify(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); }
PPC_FUNC(sub_827C9A40) { ctx.r3.u64 = active->AllocateSpecial(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_827CA050) { ctx.r3.u64 = active->AllocateGeneral(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_827C9C60) { ctx.r3.u64 = active->FreeSpecial(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_827CA0E8) { ctx.r3.u64 = active->FreeGeneral(ctx.r3.u32, ctx.r4.u32); }

int main() {
    try {
        std::array<unsigned, 4> counts{};
        auto test = [&](const Case& scenario) {
            const auto index = unsigned(scenario.kind);
            Compare(scenario, counts[index]++);
        };
        test(InitCase(1, 0, 0));
        test(InitCase(2, 0, 7));
        test(InitCase(3, 7, 7));
        test(InitCase(4, 3, 4, true));
        test(InitCase(5, 0, 0, false, true));
        test(InitCase(6, 0, 0, false, true, false, 0));
        test(InitCase(7, 0, 0, false, true, false, NewPool, 0));
        test(InitCase(8, 0, 2, false, true, false, NewPool, Data, true));
        test(InitCase(9, 0, 0, false, false, true));
        test(InitCase(10, 8, 0, false, true)); // bitmap full; byte 8 is tested
        for (unsigned bit = 0; bit < 8; ++bit) test(InitCase(100 + bit, 0, bit));
        for (unsigned byte = 1; byte < 8; ++byte) test(InitCase(110 + byte, byte, 0));
        test(ReleaseCase(11, 0, 9));
        test(ReleaseCase(12, 2, 9));
        test(ReleaseCase(13, 1, 8));
        test(ReleaseCase(14, 1, 9, false, 0));
        test(ReleaseCase(15, 1, 9, false, 1));
        test(ReleaseCase(16, 1, 9, false, 2));
        test(ReleaseCase(17, 1, 9, true, 1));
        test(ReleaseCase(18, 1, 9, true, 1, 0, true));
        test(ReleaseCase(19, 1, 10, false, 1, 0x02000000));
        test(ReleaseCase(20, 1, 10, true, 1, 0x03000000));
        for (int delta : {-1, -2, -3})
            test(ReleaseCase(200 + unsigned(-delta), 1, 9, false, 1, 0, false, delta));
        {
            auto value = ReleaseCase(210, 1, 9, true);
            value.notify_changes_data = true;
            test(value);
        }
        {
            auto value = ReleaseCase(211, 1, 9, true);
            value.data_free_changes_next = true;
            test(value);
        }
        {
            auto value = ReleaseCase(212, 1, 9, true);
            value.pool_free_reinserts_once = true;
            value.expected_trace = "NFFNFFF";
            test(value);
        }
        for (std::uint32_t flags : {0xa7820007u, 0u, 0x64800000u, 0xffffffffu}) {
            test(DispatchCase(Kind::AllocateDispatch, 21 + flags, 156, flags, 0x80000000));
            test(DispatchCase(Kind::FreeDispatch, 22 + flags, 0x3000, flags, 0xdeadbeef));
        }
        constexpr std::uint32_t Seed = 0x827b72e8;
        std::mt19937 random(Seed);
        for (unsigned n = 0; n < 256; ++n) {
            const unsigned byte = random() % 8, bit = random() % 8;
            test(InitCase(random(), byte, bit, n % 5 == 0));
            test(ReleaseCase(random(), 1 + random() % 3, n % 3 == 0 ? 9 : n % 3 == 1 ? 10 : 8,
                             n % 4 == 0, 1 + random() % 2, n % 7 == 0 ? 0x02000000 : 0));
            const auto flags = n % 2 ? 0xa7820007u : random();
            test(DispatchCase(Kind::AllocateDispatch, random(), random(), flags, random()));
            test(DispatchCase(Kind::FreeDispatch, random(), random(), flags, random()));
        }
        constexpr std::array<const char*, 4> names = {"827B72E8", "823CDCA8", "827C9D88", "827C9DB0"};
        for (unsigned i = 0; i < names.size(); ++i)
            std::printf("PASS %s %u\n", names[i], counts[i]);
        std::puts("LIMIT: synthetic callbacks and bounded low guest memory; cache sync, MMIO, concurrency and runtime scenes untested");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
