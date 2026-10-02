// Appended after the verbatim extracted PPC routine. No test-specific oracle
// implementation or guest address remapping is used.
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/ppc/memory_fill.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <windows.h>

namespace {
using namespace lo::semantic::gpu;
constexpr std::uint64_t GuestSpace = std::uint64_t{1} << 32;
constexpr std::size_t LowCommit = 0x40000;
constexpr std::uint32_t EndPage = 0xfffff000, StackTop = 0x3f000;

void Require(bool okay, const char* reason) {
    if (!okay) throw std::runtime_error(reason);
}
struct Window {
    uint8_t* base = nullptr;
    Window() {
        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS));
        Require(base != nullptr, "reserve sparse 4 GiB guest address space");
        if (VirtualAlloc(base, LowCommit, MEM_COMMIT, PAGE_READWRITE) != base ||
            VirtualAlloc(base + EndPage, 0x1000, MEM_COMMIT, PAGE_READWRITE) != base + EndPage) {
            VirtualFree(base, 0, MEM_RELEASE);
            base = nullptr;
            throw std::runtime_error("commit low/last guest pages");
        }
    }
    ~Window() { if (base) VirtualFree(base, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};
struct Case {
    std::uint32_t destination, value, bytes;
};

void Compare(Window& oracle, Window& recovered, Window& adapted,
             const Case& scenario, unsigned ordinal) {
    const auto fill_byte = std::uint8_t(scenario.value);
    const auto sentinel = fill_byte == 0xa5 ? std::uint8_t(0x5a) : std::uint8_t(0xa5);
    std::memset(oracle.base, sentinel, LowCommit);
    std::memset(oracle.base + EndPage, sentinel, 0x1000);
    std::memcpy(recovered.base, oracle.base, LowCommit);
    std::memcpy(recovered.base + EndPage, oracle.base + EndPage, 0x1000);
    std::memcpy(adapted.base, oracle.base, LowCommit);
    std::memcpy(adapted.base + EndPage, oracle.base + EndPage, 0x1000);

    PPCContext ctx{};
    const auto original_r3 = 0x1234567800000000ull | scenario.destination;
    ctx.r1.u64 = StackTop;
    ctx.lr = 0x82345678;
    ctx.r3.u64 = original_r3;
    ctx.r4.u64 = 0x7564321000000000ull | scenario.value;
    ctx.r5.u64 = 0x2345678900000000ull | scenario.bytes;
    ctx.xer.so = ordinal & 1;
    ctx.xer.ov = (ordinal >> 1) & 1;
    ctx.xer.ca = (ordinal >> 2) & 1;
    ctx.r29.u64 = 0x1122334455667788ull;
    ctx.r30.u64 = 0x99aabbccddeeff00ull;
    ctx.r31.u64 = 0x76543210fedcba98ull;
    PPCContext adapter_context{};
    std::memcpy(&adapter_context, &ctx, sizeof(ctx));
    fill_stores.clear();
    oracle_FillGuestMemory(ctx, oracle.base);
    const auto original_stores = fill_stores;
    fill_stores.clear();
    lo::semantic::gpu::ppc::FillGuestMemory(adapter_context, adapted.base);
    Require(fill_stores == original_stores, "PPC adapter store address/width/value/order");
    Require(std::memcmp(&ctx, &adapter_context, sizeof(ctx)) == 0,
            "PPC adapter complete context including volatile outputs");
    Require(ctx.r3.u64 == original_r3, "original PPC preserves the full r3 pointer");
    Require(ctx.r1.u64 == StackTop && ctx.lr == 0x82345678 &&
            ctx.r29.u64 == 0x1122334455667788ull &&
            ctx.r30.u64 == 0x99aabbccddeeff00ull &&
            ctx.r31.u64 == 0x76543210fedcba98ull, "PPC nonvolatile registers");

    GuestMemory memory(0, {recovered.base, GuestSpace});
    const auto result = FillGuestMemory(memory, scenario.destination, scenario.value, scenario.bytes);
    Require(result == scenario.destination, "recovered fill returns original pointer");
    auto inspect = [&](std::uint32_t begin, std::size_t length) {
        for (std::size_t i = 0; i < length; ++i) {
            const auto address = begin + std::uint32_t(i);
            const auto expected = std::uint32_t(address - scenario.destination) < scenario.bytes
                ? fill_byte : sentinel;
            if (oracle.base[address] != expected || recovered.base[address] != expected ||
                adapted.base[address] != expected) {
                std::fprintf(stderr,
                    "FAIL case=%u dest=%08x value=%08x bytes=%u at=%08x original=%02x recovered=%02x expected=%02x\n",
                    ordinal, scenario.destination, scenario.value, scenario.bytes,
                    address, oracle.base[address], recovered.base[address], expected);
                throw std::runtime_error("PPC/recovered fill or untouched sentinel mismatch");
            }
        }
    };
    inspect(0, LowCommit);
    inspect(EndPage, 0x1000);
}
} // namespace

int main() {
    try {
        Window oracle, recovered, adapted;
        unsigned cases = 0;
        auto test = [&](const Case& scenario) { Compare(oracle, recovered, adapted, scenario, cases++); };
        constexpr std::array<std::uint32_t, 8> Values = {
            0u, 1u, 0x7fu, 0xa5u, 0xffu, 0x1234567fu, 0xdeadbeefu, 0xffffff00u};
        for (std::uint32_t alignment = 0; alignment < 8; ++alignment)
            for (std::uint32_t length = 0; length <= 80; ++length)
                test({0x1000 + alignment, Values[(alignment + length) % Values.size()], length});

        for (const Case scenario : {
                 Case{0xfffffffc, 0x12345678, 4}, // last aligned word
                 Case{0xfffffffd, 0xdeadbeef, 7}, // unaligned prefix and wrapped word
                 Case{0xffffffff, 0xffffff00, 18}, // prefix, 16-byte group, tail
                 Case{0xfffffffe, 0x1234567f, 17},
                 Case{0xfffffffb, 0x123456aa, 6},
                 Case{0xffffffff, 0x1234567f, 0},
             }) test(scenario);

        constexpr std::uint32_t Seed = 0x82b7bc40;
        std::mt19937 random(Seed);
        for (unsigned i = 0; i < 512; ++i)
            test({0x2000 + random() % 64, random(), random() % 257});
        std::printf("PASS 82B7BC40 %u (8 alignments x 81 lengths, 6 wrap, 512 seeded random; seed=%08x)\n",
                    cases, Seed);
        std::puts("PPC adapter: full context and store address/width/value/order match in every case");
        std::puts("LIMIT: ordinary committed guest memory; no concurrent/MMIO effects or live runtime call path");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
