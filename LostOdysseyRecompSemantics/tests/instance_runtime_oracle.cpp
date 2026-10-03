#include "cpu/semantic_instance.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint32_t Object = 0x1100u;

struct Entry
{
    std::uint32_t address;
    PPCFunc* original;
    PPCFunc* wrapper;
    unsigned* fallback_calls;
};
const Entry Entries[] = {
/* ENTRY_TABLE */
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));

    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x10000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit instance memory");
        for (const Entry& entry : Entries)
        {
            const auto slot = PPC_IMAGE_BASE + PPC_IMAGE_SIZE +
                (static_cast<std::uint64_t>(entry.address - PPC_CODE_BASE) * 2u);
            if (slot + sizeof(PPCFunc*) > Space ||
                !VirtualAlloc(bytes + (slot & ~std::uint64_t{0xfff}), 0x1000,
                              MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit instance table page");
            PPC_LOOKUP_FUNC(bytes, entry.address) = entry.wrapper;
        }
    }

    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

PPCContext Seed(std::uint64_t r3)
{
    PPCContext ctx{};
    ctx.r1.u64 = 0x9000;
    ctx.r3.u64 = r3;
    ctx.r4.u64 = 0x1111222233334444ull;
    ctx.r7.u64 = 0x777788889999aaaAull;
    ctx.r8.u64 = 0x88889999aaaabbbBull;
    ctx.r9.u64 = 0x9999aaaabbbbccccull;
    ctx.r10.u64 = 0xaaaabbbbccccddddull;
    ctx.r11.u64 = 0xbbbbccccddddeeeeull;
    ctx.r12.u64 = 0xccccddddeeeeffffull;
    ctx.r31.u64 = 0x123456789abcdef0ull;
    ctx.lr = 0xfedcba9876543210ull;
    ctx.xer.so = 1;
    ctx.xer.ca = 1;
    ctx.cr6.lt = 1;
    return ctx;
}

bool Compare(const Entry& entry, bool use_table, std::uint64_t r3,
    bool enabled, Window& expected, Window& actual)
{
    std::memset(expected.bytes, 0x5a, 0x10000);
    std::memset(actual.bytes, 0x5a, 0x10000);
    PPCContext raw = Seed(r3);
    PPCContext wrapper = raw;
    entry.original(raw, expected.bytes);
    const unsigned before = *entry.fallback_calls;
    PPCFunc* call = use_table ? PPC_LOOKUP_FUNC(actual.bytes, entry.address) :
                                entry.wrapper;
    if (call != entry.wrapper)
        throw std::runtime_error("instance guest table points elsewhere");
    call(wrapper, actual.bytes);
    const bool same = *entry.fallback_calls - before == (enabled ? 0u : 1u) &&
        std::memcmp(&raw, &wrapper, sizeof(PPCContext)) == 0 &&
        std::memcmp(expected.bytes, actual.bytes, 0x10000) == 0;
    if (!same)
        std::fprintf(stderr,
            "FAIL instance %08x table %u gate %u r3 %llx/%llx cr6 %u/%u r11 %llx/%llx fallback %u\n",
            entry.address, use_table, enabled,
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(wrapper.r3.u64),
            raw.cr6.eq, wrapper.cr6.eq,
            static_cast<unsigned long long>(raw.r11.u64),
            static_cast<unsigned long long>(wrapper.r11.u64),
            *entry.fallback_calls - before);
    return same;
}
} // namespace

int main()
{
    try
    {
        const bool enabled = lo::runtime::semantic_instance::Enabled();
        Window raw, wrapper;
        unsigned comparisons = 0;
        for (std::size_t index = 0; index < std::size(Entries); ++index)
        {
            if (!Compare(Entries[index], index % 2 != 0,
                         0xabcdef0000000000ull | Object, enabled, raw, wrapper))
                return 1;
            ++comparisons;
        }
        if (!Compare(Entries[0], true, 0xdeadbeef00000000ull,
                     enabled, raw, wrapper) ||
            !Compare(Entries[3], false, 0xffffffff00000000ull,
                     enabled, raw, wrapper))
            return 1;
        comparisons += 2;

        PPCContext unknown = Seed(0xabcdef0000001100ull);
        const PPCContext before = unknown;
        if (lo::runtime::semantic_instance::Apply(unknown, wrapper.bytes,
                                                  0x82000000u) ||
            std::memcmp(&unknown, &before, sizeof(PPCContext)) != 0)
            throw std::runtime_error("unknown instance address changed context");
        std::printf("PASS instance-runtime %u PPC comparisons; gate %s\n",
                    comparisons, enabled ? "enabled" : "disabled");
        std::puts("LIMIT nine integer shapes, two null cases, full PPCContext and ordinary guest RAM; no scene or MMIO claim");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
