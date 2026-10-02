// Appended after the extracted PPC body and its extracted ABI spill helpers.
// The only oracle for CreateType9Query is oracle_CreateType9Query above.
#include "lo_semantics/query.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::size_t MemorySize = 0x40000;
constexpr GuestAddress StackTop = 0x3f000;
constexpr GuestAddress ScratchStart = 0x3e000;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Scenario {
    GuestAddress query = 0x1010;
    GuestAddress device = 0x7fffffff;
    std::array<std::int32_t, 8> results{};
    std::array<std::uint32_t, 8> counts{};
    unsigned calls = 1;
    int failure = -1;
    std::uint32_t seed = 0;
};

struct Event {
    char kind;
    GuestAddress a, b, c;
    std::int32_t result;
    bool operator==(const Event&) const = default;
};

struct Run {
    uint8_t* bytes = nullptr;
    const Scenario* scenario = nullptr;
    std::vector<Event> events;
    std::vector<std::vector<uint8_t>> callback_memory;
    unsigned init_calls = 0;

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
        callback_memory.emplace_back(bytes, bytes + ScratchStart);
    }
    GuestAddress Allocate(std::uint32_t size, std::uint32_t flags) {
        Require(size == 156 && flags == 0x64800000, "allocator arguments");
        events.push_back({'A', size, flags, 0, std::int32_t(scenario->query)});
        Snapshot();
        return scenario->query;
    }
    std::int32_t Initialize(GuestAddress device, GuestAddress owner, GuestAddress data) {
        Require(init_calls < scenario->calls && init_calls < scenario->results.size(), "unexpected init count");
        Snapshot(); // Includes all stores made by the caller before this callback.
        const unsigned index = init_calls++;
        // The external operation writes the slots. Its guest-side side effects
        // are deliberately identical for the PPC and recovered-code executions.
        Write32(owner, 0xa1000000u + index);
        Write32(data, 0xb2000000u + index);
        Write32(scenario->query + 0x98, scenario->counts[index]);
        bytes[scenario->query + 0x20 + index] = std::uint8_t(0x61 + index);
        const auto result = scenario->results[index];
        events.push_back({'I', device, owner, data, result});
        Snapshot();
        return result;
    }
    std::uint32_t Release(GuestAddress query) {
        Require(query == scenario->query, "release query address");
        Snapshot(); // Failure count store must precede release side effects.
        bytes[query + 0x91] = 0xee;
        events.push_back({'R', query, 0, 0, 0x1234});
        Snapshot();
        return 0x1234;
    }
};

Run* active_run = nullptr;

class Services final : public QueryServices {
public:
    explicit Services(Run& run) : run_(run) {}
    GuestAddress Allocate(std::uint32_t size, std::uint32_t flags) override {
        return run_.Allocate(size, flags);
    }
    std::int32_t InitializeSlot(GuestAddress device, GuestAddress owner, GuestAddress data) override {
        return run_.Initialize(device, owner, data);
    }
    std::uint32_t Release(GuestAddress query) override { return run_.Release(query); }
private:
    Run& run_;
};

void Compare(const Scenario& scenario, unsigned number) {
    auto* oracle_bytes = static_cast<uint8_t*>(VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    auto* semantic_bytes = static_cast<uint8_t*>(VirtualAlloc(nullptr, MemorySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(oracle_bytes && semantic_bytes, "allocate independent aligned guest windows");
    try {
        std::mt19937 rng(scenario.seed);
        for (std::size_t i = 0; i < MemorySize; ++i)
            oracle_bytes[i] = std::uint8_t(rng());
        if (scenario.query) {
            oracle_bytes[scenario.query + 0x08] = 0xa5;
            oracle_bytes[scenario.query + 0x11] = 0x5a;
        }
        std::memcpy(semantic_bytes, oracle_bytes, MemorySize);
        const auto sentinel_8 = oracle_bytes[scenario.query + 0x08];
        const auto sentinel_11 = oracle_bytes[scenario.query + 0x11];
        Run oracle{oracle_bytes, &scenario}, semantic{semantic_bytes, &scenario};
        PPCContext ctx{};
        ctx.r1.u64 = StackTop;
        ctx.r3.u64 = scenario.device;
        ctx.lr = 0x82345678;
        const std::array<std::uint64_t, 5> saved = {
            0x1020304050607080ull, 0x1122334455667788ull, 0x99aabbccddeeff00ull,
            0x76543210fedcba98ull, 0xabcdef1234567890ull};
        ctx.r27.u64 = saved[0]; ctx.r28.u64 = saved[1]; ctx.r29.u64 = saved[2];
        ctx.r30.u64 = saved[3]; ctx.r31.u64 = saved[4];
        active_run = &oracle;
        oracle_CreateType9Query(ctx, oracle_bytes);
        active_run = nullptr;

        Require(ctx.r1.u64 == StackTop && ctx.lr == 0x82345678, "PPC stack and LR restored");
        Require(ctx.r27.u64 == saved[0] && ctx.r28.u64 == saved[1] && ctx.r29.u64 == saved[2] &&
                ctx.r30.u64 == saved[3] && ctx.r31.u64 == saved[4], "PPC preserved registers restored");
        GuestMemory guest(0, {semantic_bytes, MemorySize});
        Services services(semantic);
        const auto result = CreateType9Query(guest, services, scenario.device);
        if (result != ctx.r3.u32 || oracle.events != semantic.events ||
            oracle.callback_memory != semantic.callback_memory ||
            !std::equal(oracle_bytes, oracle_bytes + ScratchStart, semantic_bytes)) {
            std::fprintf(stderr, "FAIL case=%u seed=%u query=%08x calls=%u failure=%d oracle=%08x recovered=%08x events=%zu/%zu\n",
                         number, scenario.seed, scenario.query, scenario.calls, scenario.failure,
                         ctx.r3.u32, result, oracle.events.size(), semantic.events.size());
            throw std::runtime_error("PPC/recovered differential mismatch");
        }
        Require(oracle.init_calls == semantic.init_calls, "callback count");
        if (scenario.query != 0) {
            Require(oracle.init_calls >= 1, "PPC do-while invokes callback once");
            Require(oracle_bytes[scenario.query + 0x08] == sentinel_8 &&
                    semantic_bytes[scenario.query + 0x08] == sentinel_8 &&
                    oracle_bytes[scenario.query + 0x11] == sentinel_11 &&
                    semantic_bytes[scenario.query + 0x11] == sentinel_11,
                    "unwritten sentinel bytes remain unchanged");
            if (scenario.failure < 0) {
                Require(oracle_bytes[scenario.query + 0x10] == 1, "success marker");
            }
        } else {
            Require(oracle.init_calls == 0, "null allocation skips initialization");
        }
    } catch (...) {
        active_run = nullptr;
        VirtualFree(oracle_bytes, 0, MEM_RELEASE);
        VirtualFree(semantic_bytes, 0, MEM_RELEASE);
        throw;
    }
    VirtualFree(oracle_bytes, 0, MEM_RELEASE);
    VirtualFree(semantic_bytes, 0, MEM_RELEASE);
}

Scenario Fixed(GuestAddress query, std::initializer_list<std::int32_t> results,
               std::initializer_list<std::uint32_t> counts, int failure, std::uint32_t seed) {
    Scenario s{};
    s.query = query; s.calls = static_cast<unsigned>(results.size()); s.failure = failure; s.seed = seed;
    std::copy(results.begin(), results.end(), s.results.begin());
    std::copy(counts.begin(), counts.end(), s.counts.begin());
    return s;
}
} // namespace

PPC_FUNC(sub_827C9D88) { ctx.r3.u64 = active_run->Allocate(ctx.r3.u32, ctx.r4.u32); }
PPC_FUNC(sub_827B72E8) { ctx.r3.s64 = active_run->Initialize(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); }
PPC_FUNC(sub_823CDCA8) { ctx.r3.u64 = active_run->Release(ctx.r3.u32); }

int main() {
    try {
        unsigned tested = 0;
        const std::array cases = {
            Fixed(0, {}, {}, -1, 1),
            Fixed(0x1010, {0}, {1}, -1, 2),
            Fixed(0x1010, {INT32_MAX}, {0}, -1, 3), // zero count still runs once
            Fixed(0x1013, {-1}, {1}, 0, 4), // unaligned guest query
            Fixed(0x1010, {INT32_MIN}, {1}, 0, 5),
            Fixed(0x1010, {0, 1, INT32_MAX}, {3, 3, 3}, -1, 6),
            Fixed(0x1010, {0, -1}, {3, 3}, 1, 7),
            Fixed(0x1010, {0, 0}, {2, 1}, -1, 8), // callback shrinks count
            Fixed(0x1010, {0, 0, 0, 0}, {4, 4, 4, 4}, -1, 9),
            Fixed(0x1010, {0, INT32_MIN}, {UINT32_MAX, UINT32_MAX}, 1, 10),
        };
        for (const auto& scenario : cases) Compare(scenario, tested++);

        constexpr std::uint32_t RandomSeed = 0x827b7408;
        std::mt19937 random(RandomSeed);
        for (unsigned n = 0; n < 512; ++n) {
            Scenario s{};
            s.seed = random();
            s.query = (n % 17 == 0) ? 0 : (0x1000 + (random() % 64));
            s.device = random();
            const unsigned target = 1 + random() % 6;
            s.calls = target;
            s.failure = (n % 5 == 0 && s.query) ? int(random() % target) : -1;
            for (unsigned i = 0; i < target; ++i) {
                s.results[i] = i == unsigned(s.failure) ? INT32_MIN :
                    (n % 3 == 0 ? INT32_MAX : 0);
                s.counts[i] = target;
            }
            Compare(s, tested++);
        }
        std::printf("PASS: %u PPC/recovered differential cases (10 fixed, 512 seeded random; seed=%08x)\n", tested, RandomSeed);
        std::puts("PASS: return, callback trace/snapshots, external guest bytes, preserved registers, r1 and LR");
        std::puts("LIMIT: stack scratch excluded from semantic memory comparison; callbacks synthetic; generated PPC is not raw XEX execution");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
