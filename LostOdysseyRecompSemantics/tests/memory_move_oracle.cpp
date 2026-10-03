// Appended after two independently extracted original PPC bodies.
#include "lo_semantics/memory_move.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <windows.h>

PPC_FUNC(sub_82B7A0B0) { oracle_CopyGuestMemory(ctx, base); }

namespace {
using namespace lo::semantic::gpu;
constexpr std::uint64_t GuestSpace = std::uint64_t{1} << 32;
constexpr std::uint32_t WindowBytes = 0x8000, SignedWindow = 0x7fffc000;
constexpr std::uint32_t StackTop = 0x7000;

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
struct Window {
    std::uint8_t* base = nullptr;
    Window() {
        base = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS));
        Require(base != nullptr, "reserve sparse 4 GiB guest space");
        if (VirtualAlloc(base, WindowBytes, MEM_COMMIT, PAGE_READWRITE) != base ||
            VirtualAlloc(base + SignedWindow, WindowBytes, MEM_COMMIT, PAGE_READWRITE) != base + SignedWindow) {
            VirtualFree(base, 0, MEM_RELEASE);
            base = nullptr;
            throw std::runtime_error("commit guest test windows");
        }
    }
    ~Window() { if (base) VirtualFree(base, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};
struct Case {
    std::uint32_t destination, source;
    std::uint64_t bytes;
    std::uint32_t stack = StackTop;
};

void Compare(Window& original, Window& recovered, const Case& test,
             bool copy, unsigned ordinal,
             const std::array<std::uint8_t, WindowBytes>& initial) {
    for (const std::uint32_t start : {0u, SignedWindow}) {
        std::memcpy(original.base + start, initial.data(), initial.size());
        std::memcpy(recovered.base + start, initial.data(), initial.size());
    }
    PPCContext ctx{};
    ctx.r1.u64 = test.stack;
    ctx.lr = 0x82345678;
    ctx.r3.u64 = 0x1234567800000000ull | test.destination;
    ctx.r4.u64 = test.source;
    ctx.r5.u64 = test.bytes;
    ctx.r29.u64 = 0x1122334455667788ull;
    ctx.r30.u64 = 0x99aabbccddeeff00ull;
    ctx.r31.u64 = 0x76543210fedcba98ull;
    const auto destination_register = ctx.r3.u64;
    if (copy) oracle_CopyGuestMemory(ctx, original.base);
    else oracle_MoveGuestMemory(ctx, original.base);
    GuestMemory memory(0, {recovered.base, GuestSpace});
    const auto result = copy
        ? CopyGuestMemory(memory, destination_register, test.source, test.bytes, test.stack)
        : MoveGuestMemory(memory, destination_register, test.source, test.bytes, test.stack);
    if (result != ctx.r3.u64) {
        std::fprintf(stderr, "FAIL %s case=%u dst=%08x src=%08x bytes=%u stack=%08x return original=%016llx recovered=%016llx\n",
            copy ? "copy" : "move", ordinal, test.destination, test.source, static_cast<std::uint32_t>(test.bytes), test.stack,
            static_cast<unsigned long long>(ctx.r3.u64), static_cast<unsigned long long>(result));
        throw std::runtime_error("full destination return register");
    }
    Require(ctx.r1.u64 == test.stack && ctx.lr == 0x82345678 &&
            ctx.r29.u64 == 0x1122334455667788ull && ctx.r30.u64 == 0x99aabbccddeeff00ull &&
            ctx.r31.u64 == 0x76543210fedcba98ull, "original leaf ABI preserved registers");
    for (const std::uint32_t start : {0u, SignedWindow}) {
        if (std::memcmp(original.base + start, recovered.base + start, WindowBytes) == 0) continue;
        for (std::uint32_t offset = 0; offset < WindowBytes; ++offset) {
            const auto address = start + offset;
            if (original.base[address] == recovered.base[address]) continue;
            std::fprintf(stderr, "FAIL %s case=%u dst=%08x src=%08x bytes=%u stack=%08x at=%08x original=%02x recovered=%02x\n",
                copy ? "copy" : "move", ordinal, test.destination, test.source, static_cast<std::uint32_t>(test.bytes), test.stack,
                address, original.base[address], recovered.base[address]);
            throw std::runtime_error("ordinary guest memory including copy spill");
        }
    }
}
} // namespace

int main() {
    try {
        Window original, recovered;
        std::array<std::uint8_t, WindowBytes> initial{};
        std::mt19937 random(0x82b7c470);
        for (auto& byte : initial) byte = static_cast<std::uint8_t>(random());
        for (const bool copy : {true, false}) {
            unsigned cases = 0;
            const auto test = [&](Case scenario) { Compare(original, recovered, scenario, copy, cases++, initial); };
            for (std::uint32_t destination = 0; destination < 8; ++destination)
                for (std::uint32_t source = 0; source < 8; ++source) {
                    for (std::uint32_t bytes = 0; bytes <= 17; ++bytes) {
                        test({0x1000 + destination, 0x3000 + source, bytes});
                        test({0x3000 + destination, 0x1000 + source, bytes});
                    }
                    for (const auto bytes : {31u, 32u, 33u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u, 257u, 511u, 1024u}) {
                        test({0x1080 + destination, 0x3080 + source, bytes});
                        test({0x3000 + destination, 0x1000 + source, bytes});
                    }
                }
            for (std::uint32_t alignment = 0; alignment < 8; ++alignment)
                for (int delta = -16; delta <= 16; ++delta)
                    for (const auto bytes : {0u, 1u, 4u, 7u, 8u, 17u, 127u, 128u, 129u, 255u, 256u, 511u})
                        test({0x2000 + alignment, static_cast<std::uint32_t>(0x2000 + alignment + delta), bytes});
            // The signed comparison crosses the 2 GiB boundary in both directions.
            for (const auto bytes : {0u, 1u, 4u, 17u, 128u, 257u})
                for (std::uint32_t alignment = 0; alignment < 8; ++alignment) {
                    test({0x7ffffff0 + alignment, 0x80000000 + alignment, bytes});
                    test({0x80000000 + alignment, 0x7ffffff0 + alignment, bytes});
                }
            // Copy's own saved r3 can be read, overwritten, or overlap either range.
            for (int delta = -16; delta <= 16; ++delta)
                for (const auto bytes : {0u, 1u, 4u, 8u, 17u, 128u}) {
                    const auto alias = static_cast<std::uint32_t>(StackTop - 8 + delta);
                    test({alias, 0x3000, bytes});
                    test({0x1000, alias, bytes});
                    test({alias, static_cast<std::uint32_t>(alias + 3), bytes});
                }
            for (unsigned index = 0; index < 256; ++index)
                test({0x2000 + random() % 128, 0x2000 + random() % 128, random() % 1025});
            for (const auto high : {0x100000000ull, 0x1234567800000000ull, 0xffffffff00000000ull})
                for (const auto low : {0u, 1u, 4u, 7u, 128u, 257u}) {
                    test({0x1003, 0x3001, high | low});
                    test({0x3003, 0x1001, high | low});
                    test({0x2000, 0x2000, high | low});
                }
            std::printf("PASS %s %u\n", copy ? "82B7A0B0" : "82B7C470", cases);
        }
        std::puts("LIMIT: ordinary committed memory and full r3 return; no complete volatile-context, access-width, fault, concurrency, MMIO, or game runtime equivalence");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
