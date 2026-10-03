// Appended after the two pinned PPC bodies and the actual save/restore helpers.
#include "lo_semantics/manager_construction.h"

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
constexpr std::uint64_t Space = std::uint64_t{1} << 32;
constexpr std::size_t LowSize = 0xa0000, PageSize = 0x1000;
constexpr GuestAddress ConstantPage = 0x82000000u;
constexpr GuestAddress GlobalPage = 0x83315000u;
constexpr GuestAddress StackTop = 0x60010u, FallbackFrame = StackTop - 128u;
constexpr GuestAddress PrimaryConstant = 0x82000fe8u;
constexpr GuestAddress SectionGlobal = 0x83315fd4u, ManagerGlobal = 0x83315fd8u;
constexpr std::uint64_t CallerLR = 0x81234567u;

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
struct GuestImage {
    std::uint8_t* bytes;
    GuestImage() {
        bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
        Require(bytes != nullptr, "reserve 4GiB guest address space");
        for (auto [address, size] : {std::pair{GuestAddress{0}, LowSize},
                                     std::pair{ConstantPage, PageSize},
                                     std::pair{GlobalPage, PageSize}})
            Require(VirtualAlloc(bytes + address, size, MEM_COMMIT, PAGE_READWRITE) ==
                    bytes + address, "commit sparse guest window");
    }
    ~GuestImage() { VirtualFree(bytes, 0, MEM_RELEASE); }
    GuestImage(const GuestImage&) = delete;
    GuestImage& operator=(const GuestImage&) = delete;
};
enum class Kind { Primary, Fallback };
struct Case {
    Kind kind;
    GuestAddress object;
    std::uint64_t object_register;
    std::uint64_t previous_register;
    bool mutate_callback;
    std::uint32_t seed;
};
struct Event {
    std::uint64_t address;
    bool operator==(const Event&) const = default;
};
struct Run {
    std::uint8_t* bytes;
    Case scenario;
    std::vector<Event> events;
    std::vector<std::array<std::uint32_t, 8>> snapshots;

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
        const auto object = scenario.object;
        snapshots.push_back({Read32(object), Read32(object + 4), Read32(object + 8),
            Read32(FallbackFrame + 80), Read32(FallbackFrame + 148),
            Read32(SectionGlobal), Read32(ManagerGlobal), Read32(0x700)});
    }
    void InitializeSection(std::uint64_t address_register) {
        Snapshot();
        events.push_back({address_register});
        Write32(0x700, Read32(0x700) ^ 0x57a7u);
        if (scenario.mutate_callback) {
            Write32(scenario.object + 8, 0xfeed1234u);
            Write32(FallbackFrame + 80, 0xaabbccddu);
            Write32(SectionGlobal, 0x87654321u);
            Write32(ManagerGlobal, 0x12345678u);
        }
        Snapshot();
    }
};
Run* active = nullptr;
struct Services final : ManagerConstructionServices {
    Run& run;
    explicit Services(Run& value) : run(value) {}
    void InitializeCriticalSection(std::uint64_t address_register) override {
        run.InitializeSection(address_register);
    }
};

void Compare(const Case& scenario, unsigned ordinal) {
    GuestImage original_image, semantic_image;
    std::mt19937 random(scenario.seed);
    for (std::size_t i = 0; i < LowSize; ++i)
        original_image.bytes[i] = std::uint8_t(random());
    for (GuestAddress page : {ConstantPage, GlobalPage})
        for (std::size_t i = 0; i < PageSize; ++i)
            original_image.bytes[page + i] = std::uint8_t(random());
    std::memcpy(semantic_image.bytes, original_image.bytes, LowSize);
    for (GuestAddress page : {ConstantPage, GlobalPage})
        std::memcpy(semantic_image.bytes + page, original_image.bytes + page, PageSize);
    Run original{original_image.bytes, scenario}, semantic{semantic_image.bytes, scenario};

    PPCContext ctx{};
    ctx.r1.u64 = StackTop;
    ctx.lr = CallerLR;
    ctx.r3.u64 = scenario.object_register;
    ctx.r4.u64 = scenario.previous_register;
    const std::array<std::uint64_t, 4> saved{
        0x1122334455667788ull, 0x2233445566778899ull,
        0x33445566778899aaull, 0x445566778899aabbull};
    ctx.r28.u64 = saved[0]; ctx.r29.u64 = saved[1];
    ctx.r30.u64 = saved[2]; ctx.r31.u64 = saved[3];
    active = &original;
    if (scenario.kind == Kind::Primary)
        oracle_ConstructPrimary(ctx, original_image.bytes);
    else
        oracle_ConstructFallback(ctx, original_image.bytes);
    active = nullptr;

    GuestMemory memory(0, {semantic_image.bytes, static_cast<std::size_t>(Space)});
    Services services(semantic);
    const auto result = scenario.kind == Kind::Primary
        ? ConstructPrimaryManager(memory, scenario.object_register, StackTop)
        : ConstructFallbackManager(memory, services, scenario.object_register,
                                   scenario.previous_register, FallbackFrame);
    if (ctx.r3.u64 != result || ctx.r1.u64 != StackTop || ctx.lr != CallerLR ||
        ctx.r28.u64 != saved[0] || ctx.r29.u64 != saved[1] ||
        ctx.r30.u64 != saved[2] || ctx.r31.u64 != saved[3] ||
        original.events != semantic.events || original.snapshots != semantic.snapshots) {
        std::fprintf(stderr,
            "case=%u kind=%u return=%016llx/%016llx sp=%016llx lr=%016llx events=%zu/%zu snapshots=%zu/%zu\n",
            ordinal, unsigned(scenario.kind), static_cast<unsigned long long>(ctx.r3.u64),
            static_cast<unsigned long long>(result), static_cast<unsigned long long>(ctx.r1.u64),
            static_cast<unsigned long long>(ctx.lr), original.events.size(),
            semantic.events.size(), original.snapshots.size(), semantic.snapshots.size());
        for (std::size_t i = 0; i < original.snapshots.size() && i < semantic.snapshots.size(); ++i)
            for (std::size_t j = 0; j < original.snapshots[i].size(); ++j)
                if (original.snapshots[i][j] != semantic.snapshots[i][j])
                    std::fprintf(stderr, "snapshot %zu field %zu: %08x/%08x\n", i, j,
                                 original.snapshots[i][j], semantic.snapshots[i][j]);
        std::fprintf(stderr, "nonvolatile %016llx/%016llx/%016llx/%016llx\n",
            static_cast<unsigned long long>(ctx.r28.u64), static_cast<unsigned long long>(ctx.r29.u64),
            static_cast<unsigned long long>(ctx.r30.u64), static_cast<unsigned long long>(ctx.r31.u64));
        throw std::runtime_error("return, ABI register, or callback trace mismatch");
    }

    const std::size_t abi_begin = FallbackFrame - 64u;
    const std::size_t abi_end = StackTop + 160u;
    for (std::size_t i = 0; i < LowSize; ++i) {
        if (scenario.kind == Kind::Fallback && i >= abi_begin && i < abi_end)
            continue;
        if (original_image.bytes[i] != semantic_image.bytes[i]) {
            std::fprintf(stderr, "case=%u first low byte=%zx original=%02x semantic=%02x\n",
                         ordinal, i, original_image.bytes[i], semantic_image.bytes[i]);
            throw std::runtime_error("guest low memory mismatch");
        }
    }
    for (GuestAddress page : {ConstantPage, GlobalPage})
        Require(std::memcmp(original_image.bytes + page, semantic_image.bytes + page,
                            PageSize) == 0, "guest high page mismatch");
    if (scenario.kind == Kind::Fallback) {
        for (GuestAddress address : {scenario.object, scenario.object + 4u,
                                     scenario.object + 8u, FallbackFrame + 80u,
                                     FallbackFrame + 148u})
            Require(original.Read32(address) == semantic.Read32(address),
                    "fallback owned word in ABI frame mismatch");
    }
}
} // namespace

PPC_FUNC(__imp__RtlInitializeCriticalSection) {
    active->InitializeSection(ctx.r3.u64);
    ctx.r3.u64 = 0xfedcba9876543210ull; // The constructor restores its own r3.
}

int main() {
    try {
        unsigned primary = 0, fallback = 0;
        auto test = [&](Case scenario) {
            Compare(scenario, scenario.kind == Kind::Primary ? primary++ : fallback++);
        };
        const GuestAddress high_alias = StackTop - 16u - 0x20dd8u;
        const GuestAddress low_alias = StackTop - 12u - 0x20dd8u;
        const GuestAddress zero_alias = StackTop - 16u - 0x20dbcu;
        for (GuestAddress object : {0x10000u, high_alias, low_alias, zero_alias})
            test({Kind::Primary, object, 0x1234567800000000ull | object, 0,
                  false, 0x5970u + primary});
        for (unsigned i = 0; i < 24; ++i) {
            const GuestAddress object = 0x10000u + (i % 8u) * 0x200u;
            test({Kind::Primary, object, (std::uint64_t(i) << 32) | object,
                  0, false, 0x597100u + i});
        }
        for (GuestAddress object : {0x10000u, 0x20000u, FallbackFrame + 76u})
            for (bool mutate : {false, true})
                test({Kind::Fallback, object, 0xabcdef1200000000ull | object,
                      0xfeedface8330b608ull, mutate, 0x4ed0u + fallback});
        for (unsigned i = 0; i < 24; ++i) {
            const GuestAddress object = 0x10000u + (i % 8u) * 0x100u;
            test({Kind::Fallback, object, (std::uint64_t(i) << 32) | object,
                  0xcafebabe00000000ull | i, (i & 1u) != 0, 0x4ed100u + i});
        }
        std::printf("PASS 827C5970 %u\nPASS 827C4ED0 %u\n", primary, fallback);
        std::puts("LIMIT generated PPC, synthetic critical-section callback; ABI save/backchain scratch excluded");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
