// Appended after five verbatim generated PPC wrapper bodies.
#include "lo_semantics/memory_services.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::size_t AddressSpaceSize = std::size_t{1} << 32;
constexpr GuestAddress LowEnd = 0x40000, ScratchStart = 0x3e000, StackTop = 0x3f000;
constexpr GuestAddress HeapPage = 0x83245000, ProtectPage = 0x83247000;
constexpr GuestAddress HeapGlobal = 0x83245708, ProtectGlobal = 0x83247200;
constexpr std::size_t PageSize = 0x1000;

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

struct Image {
    std::uint8_t* base = nullptr;
    Image() {
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, AddressSpaceSize,
            MEM_RESERVE, PAGE_NOACCESS));
        Require(base != nullptr, "reserve 4GiB guest address space");
        for (const auto [address, size] : {std::pair{GuestAddress{0}, std::size_t{LowEnd}},
                                            std::pair{HeapPage, PageSize},
                                            std::pair{ProtectPage, PageSize}}) {
            if (VirtualAlloc(base + address, size, MEM_COMMIT, PAGE_READWRITE) == nullptr) {
                VirtualFree(base, 0, MEM_RELEASE);
                base = nullptr;
                throw std::runtime_error("commit a guest test window");
            }
        }
    }
    ~Image() { if (base) VirtualFree(base, 0, MEM_RELEASE); }
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
};

enum class Kind { AllocatePhysical, FreePhysical, AllocateHeap, FreeHeap, GetHeap };
struct Scenario {
    Kind kind;
    std::uint32_t seed, a = 0, b = 0, c = 0, d = 0;
    std::uint32_t protect_global = 0, heap_global = 0;
    std::uint32_t callback_result = 0;
};
struct Event {
    char kind;
    std::uint32_t a, b, c, d, e, f, result;
    bool operator==(const Event&) const = default;
};

struct Run {
    std::uint8_t* base;
    const Scenario* scenario;
    std::vector<Event> events;
    std::vector<std::vector<std::uint8_t>> snapshots;

    void Write32(GuestAddress at, std::uint32_t value) {
        base[at] = std::uint8_t(value >> 24);
        base[at + 1] = std::uint8_t(value >> 16);
        base[at + 2] = std::uint8_t(value >> 8);
        base[at + 3] = std::uint8_t(value);
    }
    std::vector<std::uint8_t> MappedBytes() const {
        std::vector<std::uint8_t> result;
        result.reserve(ScratchStart + 2 * PageSize);
        result.insert(result.end(), base, base + ScratchStart);
        result.insert(result.end(), base + HeapPage, base + HeapPage + PageSize);
        result.insert(result.end(), base + ProtectPage, base + ProtectPage + PageSize);
        return result;
    }
    void Snapshot() { snapshots.push_back(MappedBytes()); }
    void MarkCallback(std::uint8_t bit) {
        base[0x500] ^= bit;
        base[HeapGlobal + 3] ^= bit;
    }

    GuestAddress AllocatePhysical(std::uint32_t type, std::uint32_t bytes,
        std::uint32_t protect, GuestAddress minimum, GuestAddress maximum,
        std::uint32_t alignment) {
        Snapshot();
        MarkCallback(1);
        events.push_back({'P', type, bytes, protect, minimum, maximum,
                          alignment, scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    void FreePhysical(std::uint32_t type, GuestAddress address) {
        Snapshot();
        MarkCallback(2);
        events.push_back({'p', type, address, 0, 0, 0, 0, 0});
        Snapshot();
    }
    void ReportAllocationFailure(std::uint32_t code) {
        Snapshot();
        MarkCallback(4);
        events.push_back({'E', code, 0, 0, 0, 0, 0, 0});
        Snapshot();
    }
    GuestAddress AllocateHeap(GuestAddress heap, std::uint32_t flags,
        std::uint32_t bytes) {
        Snapshot();
        MarkCallback(8);
        events.push_back({'H', heap, flags, bytes, 0, 0, 0,
                          scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
    std::uint32_t FreeHeap(GuestAddress heap, std::uint32_t flags,
        GuestAddress address) {
        Snapshot();
        MarkCallback(16);
        events.push_back({'h', heap, flags, address, 0, 0, 0,
                          scenario->callback_result});
        Snapshot();
        return scenario->callback_result;
    }
};

Run* active = nullptr;
struct Services final : KernelMemoryServices {
    Run& run;
    explicit Services(Run& value) : run(value) {}
    GuestAddress AllocatePhysical(std::uint32_t type, std::uint32_t bytes,
        std::uint32_t protect, GuestAddress minimum, GuestAddress maximum,
        std::uint32_t alignment) override {
        return run.AllocatePhysical(type, bytes, protect, minimum, maximum, alignment);
    }
    void FreePhysical(std::uint32_t type, GuestAddress address) override {
        run.FreePhysical(type, address);
    }
    void ReportAllocationFailure(std::uint32_t code) override {
        run.ReportAllocationFailure(code);
    }
    GuestAddress AllocateHeap(GuestAddress heap, std::uint32_t flags,
        std::uint32_t bytes) override { return run.AllocateHeap(heap, flags, bytes); }
    std::uint32_t FreeHeap(GuestAddress heap, std::uint32_t flags,
        GuestAddress address) override { return run.FreeHeap(heap, flags, address); }
};

PPCFunc* OracleFunction(Kind kind) {
    switch (kind) {
    case Kind::AllocatePhysical: return oracle_AllocatePhysical;
    case Kind::FreePhysical: return oracle_FreePhysical;
    case Kind::AllocateHeap: return oracle_AllocateHeap;
    case Kind::FreeHeap: return oracle_FreeHeap;
    case Kind::GetHeap: return sub_823ACC98;
    }
    throw std::runtime_error("unknown wrapper kind");
}

std::uint32_t SemanticCall(const Scenario& scenario, Run& run) {
    GuestMemory memory(0, {run.base, AddressSpaceSize});
    Services services(run);
    switch (scenario.kind) {
    case Kind::AllocatePhysical:
        return AllocatePhysicalMemory(memory, services, scenario.a,
            scenario.b, scenario.c, scenario.d);
    case Kind::FreePhysical:
        FreePhysicalMemory(services, scenario.a);
        return 0; // The original import's residual r3 is deliberately not compared.
    case Kind::AllocateHeap:
        return AllocateHeapMemory(memory, services, scenario.a, scenario.b);
    case Kind::FreeHeap:
        return FreeHeapMemory(memory, services, scenario.a);
    case Kind::GetHeap:
        return GetProcessHeap(memory);
    }
    throw std::runtime_error("unknown wrapper kind");
}

void Compare(const Scenario& scenario, unsigned ordinal) {
    Image oracle_image, semantic_image;
    std::mt19937 random(scenario.seed);
    for (GuestAddress address = 0; address < LowEnd; ++address)
        oracle_image.base[address] = std::uint8_t(random());
    for (GuestAddress address : {HeapPage, ProtectPage})
        for (std::size_t offset = 0; offset < PageSize; ++offset)
            oracle_image.base[address + offset] = std::uint8_t(random());
    std::memcpy(semantic_image.base, oracle_image.base, LowEnd);
    std::memcpy(semantic_image.base + HeapPage, oracle_image.base + HeapPage, PageSize);
    std::memcpy(semantic_image.base + ProtectPage, oracle_image.base + ProtectPage, PageSize);
    Run oracle{oracle_image.base, &scenario}, semantic{semantic_image.base, &scenario};
    oracle.Write32(ProtectGlobal, scenario.protect_global);
    oracle.Write32(HeapGlobal, scenario.heap_global);
    semantic.Write32(ProtectGlobal, scenario.protect_global);
    semantic.Write32(HeapGlobal, scenario.heap_global);

    PPCContext ctx{};
    ctx.r1.u64 = StackTop;
    ctx.lr = 0x82345678;
    ctx.r3.u64 = scenario.a; ctx.r4.u64 = scenario.b;
    ctx.r5.u64 = scenario.c; ctx.r6.u64 = scenario.d;
    ctx.r30.u64 = 0x1122334455667788ull;
    ctx.r31.u64 = 0x2233445566778899ull;
    active = &oracle;
    OracleFunction(scenario.kind)(ctx, oracle_image.base);
    active = nullptr;
    Require(ctx.r1.u64 == StackTop && ctx.lr == 0x82345678 &&
            ctx.r30.u64 == 0x1122334455667788ull &&
            ctx.r31.u64 == 0x2233445566778899ull,
            "original PPC preserved stack, LR and nonvolatile registers");
    const std::uint32_t semantic_result = SemanticCall(scenario, semantic);
    if ((scenario.kind != Kind::FreePhysical && ctx.r3.u32 != semantic_result) ||
        oracle.events != semantic.events || oracle.snapshots != semantic.snapshots ||
        oracle.MappedBytes() != semantic.MappedBytes()) {
        std::fprintf(stderr, "FAIL kind=%u case=%u seed=%08x return=%08x/%08x events=%zu/%zu\n",
            unsigned(scenario.kind), ordinal, scenario.seed, ctx.r3.u32,
            semantic_result, oracle.events.size(), semantic.events.size());
        throw std::runtime_error("original PPC / recovered memory wrapper mismatch");
    }
}
} // namespace

PPC_FUNC(__imp__MmAllocatePhysicalMemoryEx) {
    ctx.r3.u64 = active->AllocatePhysical(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32,
        ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
}
PPC_FUNC(__imp__MmFreePhysicalMemory) {
    active->FreePhysical(ctx.r3.u32, ctx.r4.u32);
    ctx.r3.u64 = 0xdecafbad; // Opaque residual register, not a semantic result.
}
PPC_FUNC(sub_822CA180) { active->ReportAllocationFailure(ctx.r3.u32); }
PPC_FUNC(sub_823ACCB0) {
    ctx.r3.u64 = active->AllocateHeap(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}
PPC_FUNC(sub_823ADE28) {
    ctx.r3.u64 = active->FreeHeap(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}

int main() {
    try {
        std::array<unsigned, 5> counts{};
        auto test = [&](const Scenario& scenario) {
            const auto index = unsigned(scenario.kind);
            Compare(scenario, counts[index]++);
        };
        test({Kind::AllocatePhysical, 1, 0, 0xffffffffu, 0, 0x20000000u, 0, 0, 0});
        test({Kind::AllocatePhysical, 2, 0x1000, 0xffffffffu, 0x2000, 0xffffffffu, 1, 0, 0x87654321});
        test({Kind::AllocatePhysical, 3, 0x40, 0xfffffff0u, 0x1000, 0x20000000u, 0, 0, 0});
        test({Kind::AllocatePhysical, 4, 1, 0x80000000u, 0, 0x20000001u, 1, 0, 0x80000000});
        test({Kind::FreePhysical, 5, 0});
        test({Kind::FreePhysical, 6, 0xf1234567u});
        test({Kind::AllocateHeap, 7, 0, 0, 0, 0, 0, 0xf2345678u, 0});
        test({Kind::AllocateHeap, 8, 0x40, 0xffffffffu, 0, 0, 0, 0x81234567u, 0xc1234567u});
        test({Kind::AllocateHeap, 9, 0x80, 0x1000, 0, 0, 0, 0x80000000u, 0});
        test({Kind::FreeHeap, 10, 0, 0, 0, 0, 0, 0xfedcba98u, 0});
        test({Kind::FreeHeap, 11, 0xe1234567u, 0, 0, 0, 0, 0x81234567u, 1});
        test({Kind::GetHeap, 12, 0, 0, 0, 0, 0, 0xfedcba98u});

        constexpr std::uint32_t Seed = 0x827c9e20;
        std::mt19937 random(Seed);
        for (unsigned i = 0; i < 32; ++i) {
            test({Kind::AllocatePhysical, random(), random(), i % 2 ? 0xffffffffu : random(),
                  random(), random(), i % 3, random(), i % 4 ? random() : 0});
            test({Kind::FreePhysical, random(), random()});
            test({Kind::AllocateHeap, random(), random(), random(), 0, 0, 0, random(), random()});
            test({Kind::FreeHeap, random(), random(), 0, 0, 0, 0, random(), i % 2});
            test({Kind::GetHeap, random(), 0, 0, 0, 0, 0, random()});
        }
        constexpr std::array<const char*, 5> names{
            "827C9E20", "827C9EB8", "827CAD38", "827CAD80", "823ACC98"};
        for (unsigned i = 0; i < counts.size(); ++i)
            std::printf("PASS %s %u\n", names[i], counts[i]);
        std::puts("LIMIT generated PPC, synthetic kernel/heap callbacks, sparse committed guest windows; void physical-free r3 ignored");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
