// Appended after original generated PPC bodies and the real ABI spill helpers.
#include "lo_semantics/allocation_backend.h"

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
constexpr std::uint64_t GuestSpace = std::uint64_t{1} << 32;
constexpr std::size_t LowCommit = 0x40000, LowCompare = 0x3e000;
constexpr std::uint32_t StackTop = 0x3f000;
constexpr std::uint32_t TablePage = 0x831e7000, Table = 0x831e7824;
constexpr std::array<std::uint32_t, 4> Policies = {
    0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00};

enum class Kind { Allocate, Free };
struct Case {
    Kind kind;
    std::uint32_t seed, arg, flags, callback_result;
    std::string expected_trace;
    std::uint32_t expected_alignment = 0;
    std::uint32_t expected_protection = 0;
};
struct Event {
    char kind;
    std::uint32_t a, b, c, d, result;
    bool operator==(const Event&) const = default;
};
void Require(bool okay, const char* reason) {
    if (!okay) throw std::runtime_error(reason);
}
struct Run {
    uint8_t* base;
    const Case* scenario;
    std::vector<Event> events;
    std::vector<std::vector<uint8_t>> snapshots;

    void Write32(std::uint32_t address, std::uint32_t value) {
        base[address] = std::uint8_t(value >> 24);
        base[address + 1] = std::uint8_t(value >> 16);
        base[address + 2] = std::uint8_t(value >> 8);
        base[address + 3] = std::uint8_t(value);
    }
    void Snapshot() {
        auto& bytes = snapshots.emplace_back();
        bytes.insert(bytes.end(), base, base + LowCompare);
        bytes.insert(bytes.end(), base + TablePage, base + TablePage + 0x1000);
    }
    GuestAddress AllocatePhysical(std::uint32_t bytes, GuestAddress request,
                                  std::uint32_t alignment, std::uint32_t protection) {
        Snapshot();
        base[0x410] ^= 0x10;
        events.push_back({'P', bytes, request, alignment, protection, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    GuestAddress AllocateHeap(std::uint32_t flags, std::uint32_t bytes) {
        Snapshot();
        base[0x410] ^= 0x20;
        events.push_back({'H', flags, bytes, 0, 0, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    void Fill(GuestAddress address, std::uint8_t value, std::uint32_t bytes) {
        Snapshot();
        base[0x410] ^= 0x40;
        events.push_back({'Z', address, value, bytes, 0, 0});
        Snapshot();
    }
    std::uint32_t FreePhysical(GuestAddress address) {
        Snapshot();
        base[0x410] ^= 0x80;
        events.push_back({'p', address, 0, 0, 0, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    std::uint32_t FreeHeap(GuestAddress address) {
        Snapshot();
        base[0x410] ^= 0x08;
        events.push_back({'h', address, 0, 0, 0, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
};
Run* active = nullptr;
struct Services final : GeneralAllocationServices {
    Run& run;
    explicit Services(Run& value) : run(value) {}
    GuestAddress AllocatePhysical(std::uint32_t bytes, GuestAddress request,
                                  std::uint32_t alignment, std::uint32_t protection) override {
        return run.AllocatePhysical(bytes, request, alignment, protection);
    }
    GuestAddress AllocateHeap(std::uint32_t flags, std::uint32_t bytes) override {
        return run.AllocateHeap(flags, bytes);
    }
    void Fill(GuestAddress address, std::uint8_t value, std::uint32_t bytes) override {
        run.Fill(address, value, bytes);
    }
    std::uint32_t FreePhysical(GuestAddress address) override { return run.FreePhysical(address); }
    std::uint32_t FreeHeap(GuestAddress address) override { return run.FreeHeap(address); }
};

struct GuestWindow {
    uint8_t* base;
    GuestWindow() {
        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS));
        Require(base != nullptr, "reserve 4 GiB guest address space");
        Require(VirtualAlloc(base, LowCommit, MEM_COMMIT, PAGE_READWRITE) == base,
                "commit low guest memory and stack");
        Require(VirtualAlloc(base + TablePage, 0x1000, MEM_COMMIT, PAGE_READWRITE) == base + TablePage,
                "commit original high guest policy table page");
    }
    ~GuestWindow() { if (base) VirtualFree(base, 0, MEM_RELEASE); }
    GuestWindow(const GuestWindow&) = delete;
    GuestWindow& operator=(const GuestWindow&) = delete;
};

void Compare(const Case& scenario, unsigned ordinal) {
    GuestWindow oracle_memory, recovered_memory;
    std::mt19937 random(scenario.seed);
    for (std::size_t i = 0; i < LowCommit; ++i) oracle_memory.base[i] = std::uint8_t(random());
    for (std::size_t i = 0; i < 0x1000; ++i)
        oracle_memory.base[TablePage + i] = std::uint8_t(random());
    Run oracle{oracle_memory.base, &scenario}, recovered{recovered_memory.base, &scenario};
    for (unsigned i = 0; i < Policies.size(); ++i) oracle.Write32(Table + 4 * i, Policies[i]);
    std::memcpy(recovered_memory.base, oracle_memory.base, LowCommit);
    std::memcpy(recovered_memory.base + TablePage, oracle_memory.base + TablePage, 0x1000);

    PPCContext ctx{};
    ctx.r1.u64 = StackTop; ctx.lr = 0x82345678;
    ctx.r3.u64 = scenario.arg; ctx.r4.u64 = scenario.flags;
    constexpr std::array<std::uint64_t, 3> saved = {
        0x1122334455667788ull, 0x99aabbccddeeff00ull, 0x76543210fedcba98ull};
    ctx.r29.u64 = saved[0]; ctx.r30.u64 = saved[1]; ctx.r31.u64 = saved[2];
    active = &oracle;
    if (scenario.kind == Kind::Allocate)
        oracle_AllocateGeneral(ctx, oracle_memory.base);
    else
        oracle_FreeGeneral(ctx, oracle_memory.base);
    active = nullptr;
    Require(ctx.r1.u64 == StackTop && ctx.lr == 0x82345678,
            "original PPC r1 and LR preserved");
    Require(ctx.r29.u64 == saved[0] && ctx.r30.u64 == saved[1] && ctx.r31.u64 == saved[2],
            "original PPC nonvolatile registers preserved");
    std::string trace;
    for (const auto& event : oracle.events) trace += event.kind;
    Require(trace == scenario.expected_trace, "original PPC callback branch/order");
    if (scenario.expected_alignment) {
        Require(!oracle.events.empty() && oracle.events[0].kind == 'P' &&
                oracle.events[0].c == scenario.expected_alignment &&
                oracle.events[0].d == scenario.expected_protection,
                "original PPC reads high guest table and alignment field");
    }
    GuestMemory guest(0, {recovered_memory.base, GuestSpace});
    Services services(recovered);
    const auto result = scenario.kind == Kind::Allocate
        ? AllocateGeneral(guest, services, scenario.arg, scenario.flags)
        : FreeGeneral(services, scenario.arg, scenario.flags);
    if (ctx.r3.u32 != result || oracle.events != recovered.events ||
        oracle.snapshots != recovered.snapshots ||
        !std::equal(oracle_memory.base, oracle_memory.base + LowCompare, recovered_memory.base) ||
        !std::equal(oracle_memory.base + TablePage, oracle_memory.base + TablePage + 0x1000,
                    recovered_memory.base + TablePage)) {
        std::fprintf(stderr,
            "FAIL kind=%u case=%u seed=%08x arg=%08x flags=%08x callback=%08x return=%08x/%08x events=%zu/%zu\n",
            unsigned(scenario.kind), ordinal, scenario.seed, scenario.arg, scenario.flags,
            scenario.callback_result, ctx.r3.u32, result, oracle.events.size(), recovered.events.size());
        throw std::runtime_error("original PPC / recovered C++ mismatch");
    }
}
Case Alloc(std::uint32_t seed, std::uint32_t size, std::uint32_t flags,
           std::uint32_t result, const char* trace,
           std::uint32_t alignment = 0, std::uint32_t protection = 0) {
    return {Kind::Allocate, seed, size, flags, result, trace, alignment, protection};
}
Case Free(std::uint32_t seed, std::uint32_t address, std::uint32_t flags,
          std::uint32_t result, const char* trace) {
    return {Kind::Free, seed, address, flags, result, trace};
}
} // namespace

PPC_FUNC(sub_827C9E20) {
    ctx.r3.u64 = active->AllocatePhysical(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
}
PPC_FUNC(sub_827CAD38) { ctx.r3.u64 = active->AllocateHeap(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_82B7BC40) { active->Fill(ctx.r3.u32, ctx.r4.u8, ctx.r5.u32); }
PPC_FUNC(sub_827C9EB8) { ctx.r3.u64 = active->FreePhysical(ctx.r3.u32); }
PPC_FUNC(sub_827CAD80) { ctx.r3.u64 = active->FreeHeap(ctx.r3.u32); }

int main() {
    try {
        unsigned allocate_cases = 0, free_cases = 0;
        auto test = [&](const Case& scenario) {
            Compare(scenario, scenario.kind == Kind::Allocate ? allocate_cases++ : free_cases++);
        };
        test(Alloc(1, 156, 0, 0x80000123, "H"));
        test(Alloc(2, 0xffffffff, 0x40000000, 0, "H"));
        test(Alloc(3, 0, 0x7fffffff, 0x3000, "H"));
        test(Alloc(4, 156, 0x80000000, 0x2100, "P", 4096, Policies[0]));
        test(Alloc(5, 4096, 0x91000000, 0x2200, "P", 2, Policies[1]));
        test(Alloc(6, 0xffffffff, 0xaf000000, 0xf0000100, "P", 32768, Policies[2]));
        test(Alloc(7, 0, 0xb1000000, 0x2300, "P", 2, Policies[3]));
        test(Alloc(8, 156, 0xc0000000, 0x2400, "PZ", 4096, Policies[0]));
        test(Alloc(9, 156, 0xc0000000, 0, "P", 4096, Policies[0]));
        test(Alloc(10, 0xffffffff, 0xf0000000, 0xf0000100, "PZ", 4096, Policies[3]));
        test(Free(11, 0, 0x80000000, 0xdeadbeef, ""));
        test(Free(12, 0xf0000100, 0x80000000, 0xdeadbeef, "p"));
        test(Free(13, 0, 0, 0x80000000, "h"));
        test(Free(14, 0, 0x40000000, 0x12345678, "h"));
        test(Free(15, 0x3100, 0xffffffff, 0xffffffff, "p"));
        constexpr std::uint32_t Seed = 0x827ca050;
        std::mt19937 random(Seed);
        for (unsigned n = 0; n < 512; ++n) {
            const bool physical = (n & 1) != 0;
            const std::uint32_t flags = (random() & 0x7fffffff) | (physical ? 0x80000000 : 0);
            const auto allocation_result = n % 7 == 0 ? 0 : random();
            const auto alloc_trace = physical ?
                (allocation_result && (flags & 0x40000000) ? "PZ" : "P") : "H";
            test(Alloc(random(), random(), flags, allocation_result, alloc_trace));
            const auto address = n % 7 == 0 ? 0 : random();
            test(Free(random(), address, flags, random(),
                      physical ? (address ? "p" : "") : "h"));
        }
        std::printf("PASS 827CA050 %u\n", allocate_cases);
        std::printf("PASS 827CA0E8 %u\n", free_cases);
        std::puts("LIMIT: generated PPC oracle, synthetic allocator and fill callbacks; committed low pages plus original high policy table only");
        return 0;
    } catch (const std::exception& error) {
        active = nullptr;
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
