// Appended after the original PPC bodies and their real ABI helpers.
#include "lo_semantics/manager_storage.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <windows.h>

PPC_FUNC(sub_827C5B30) { oracle_InitializeStorageBuckets(ctx, base); }

namespace {
using namespace lo::semantic::gpu;
constexpr std::uint64_t GuestSpace = std::uint64_t{1} << 32;
constexpr std::uint32_t RegionSize = 0x100000, HighRegion = 0x90000000;
constexpr std::uint32_t StackTop = 0xf0000, ScratchBegin = StackTop - 144;
constexpr std::uint32_t CallerLR = 0x81234567;

void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
struct Window {
    std::uint8_t* bytes = nullptr;
    Window() {
        bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS));
        Require(bytes != nullptr, "reserve guest address space");
        if (VirtualAlloc(bytes, RegionSize, MEM_COMMIT, PAGE_READWRITE) != bytes ||
            VirtualAlloc(bytes + HighRegion, RegionSize, MEM_COMMIT, PAGE_READWRITE) != bytes + HighRegion) {
            VirtualFree(bytes, 0, MEM_RELEASE);
            bytes = nullptr;
            throw std::runtime_error("commit test regions");
        }
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

void CheckPool(const GuestMemory& memory, GuestAddress pool) {
    const auto head = pool + 0x28000;
    auto node = memory.ReadU32(head);
    auto owner = head;
    // Address zero is also the null link: an actual node there is not linked
    // by its successor in the original, despite its payload being written.
    for (unsigned remaining = 8192; remaining > (pool == 0 ? 1u : 0u); --remaining) {
        Require(node == pool + (remaining - 1) * 20, "pool chain contains each node in reverse insertion order");
        Require(memory.ReadU32(node) == 0 && memory.ReadU32(node + 4) == 0 &&
                memory.ReadU32(node + 8) == 0, "pool node payload starts empty");
        Require(memory.ReadU32(node + 16) == owner, "pool predecessor link points to owning link field");
        owner = node + 12;
        node = memory.ReadU32(node + 12);
    }
    Require(node == 0, "pool chain terminates at its null link");
}

void Compare(Window& original, Window& recovered, bool manager,
             std::uint64_t input, unsigned seed, unsigned ordinal) {
    std::mt19937 random(seed);
    for (const auto region : {0u, HighRegion}) {
        for (std::uint32_t offset = 0; offset < RegionSize; ++offset)
            original.bytes[region + offset] = static_cast<std::uint8_t>(random());
        std::memcpy(recovered.bytes + region, original.bytes + region, RegionSize);
    }
    PPCContext ctx{};
    ctx.r1.u64 = StackTop;
    ctx.lr = CallerLR;
    ctx.r3.u64 = input;
    std::array<PPCRegister*,6> registers{&ctx.r26, &ctx.r27, &ctx.r28, &ctx.r29, &ctx.r30, &ctx.r31};
    for (unsigned i = 0; i < registers.size(); ++i) registers[i]->u64 = 0x1122334455667700ull + i;
    if (manager) oracle_InitializePrimaryManagerStorage(ctx, original.bytes);
    else oracle_InitializeStorageBuckets(ctx, original.bytes);
    GuestMemory memory(0, {recovered.bytes, GuestSpace});
    const auto result = manager ? InitializePrimaryManagerStorage(memory, input)
                                : InitializeStorageBuckets(memory, input);
    Require(result == ctx.r3.u64, "full r3 return including manager pool adjustment");
    Require(ctx.r1.u64 == StackTop && ctx.lr == CallerLR, "original ABI restores stack and LR");
    for (unsigned i = 0; i < registers.size(); ++i)
        Require(registers[i]->u64 == 0x1122334455667700ull + i, "original nonvolatile register restoration");
    // This semantic API has no guest locals. Only the original prologue's
    // backchain and nonvolatile-register save bytes are outside its contract.
    if (manager) std::memcpy(recovered.bytes + ScratchBegin, original.bytes + ScratchBegin, 144);
    for (const auto region : {0u, HighRegion}) {
        if (std::memcmp(original.bytes + region, recovered.bytes + region, RegionSize) == 0) continue;
        for (std::uint32_t offset = 0; offset < RegionSize; ++offset) {
            const auto address = region + offset;
            if (original.bytes[address] == recovered.bytes[address]) continue;
            std::fprintf(stderr, "FAIL %s case=%u input=%016llx at=%08x original=%02x recovered=%02x\n",
                manager ? "manager" : "pool", ordinal, static_cast<unsigned long long>(input),
                address, original.bytes[address], recovered.bytes[address]);
            throw std::runtime_error("ordinary memory or untouched bytes mismatch");
        }
    }
    const auto object = static_cast<GuestAddress>(input);
    CheckPool(memory, manager ? object + 0x20de0 : object);
    if (manager) {
        Require(memory.ReadU32(object + 0x20dbc) == 1, "manager enabled flag");
        Require(memory.ReadU32(object + 3364) == 0xffffffffu, "initial sentinel field");
        Require(memory.ReadU32(object + 3512) == 0, "zero-byte lookup maps first class");
        const auto last_class = memory.ReadU32(object + 3512 + 32768 * 4);
        Require(last_class < 42, "32 KiB lookup remains inside 42 classes");
        for (unsigned group = 0; group < 4; ++group)
            for (unsigned size = 0; size <= 32768; ++size) {
                const auto index = memory.ReadU32(object + 3512 + size * 4);
                Require(memory.ReadU32(object + group * 840 + index * 20 + 20) >= size,
                        "chosen size class fits requested size");
                if (index != 0)
                    Require(memory.ReadU32(object + group * 840 + (index - 1) * 20 + 20) < size,
                            "chosen size class is minimal");
            }
    }
}
} // namespace

int main() {
    try {
        Window original, recovered;
        for (const bool manager : {false, true}) {
            unsigned cases = 0;
            for (const auto object : {0u, 0x1000u, 0x10003u, 0x30000u, HighRegion + 0x1000u, HighRegion + 0x10007u})
                for (const auto high : {0ull, 0x1234567800000000ull, 0xffffffff00000000ull}) {
                    Compare(original, recovered, manager, high | object,
                            0x827c5d88u + cases * 0x9e37u, cases);
                    ++cases;
                }
            std::printf("PASS %s %u\n", manager ? "827C5D88" : "827C5B30", cases);
        }
        std::puts("LIMIT: bounded original-generated-PPC memory/return comparison; generic ABI scratch excluded; no runtime, MMIO, fault or concurrency proof");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
