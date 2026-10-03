#include "lo_semantics/registered_metadata_initializers.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
using lo::semantic::gpu::GuestAddress;
using lo::semantic::gpu::GuestMemory;
namespace initializer = lo::semantic::gpu::registered_metadata_initializers;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress AliasObject = 0x821894A8u;
constexpr std::array<GuestAddress, 4> Pages = {
    0x10000u, 0x82000000u, 0x82189000u, 0x83235000u,
};

struct Entry
{
    GuestAddress address;
    PPCFunc* original;
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
        if (!bytes)
            throw std::runtime_error("reserve initializer guest space");
        for (GuestAddress page : Pages)
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit initializer guest page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct HostFpGuard
{
    PPCFPSCRRegister control{};
    std::uint32_t saved = control.getcsr();
    ~HostFpGuard() { control.setcsr(saved); }
};

void Initialize(Window& window, std::uint32_t f0_word)
{
    for (GuestAddress page : Pages)
        std::memset(window.bytes + page, 0xbd, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(0x82000E50u, f0_word);
    memory.WriteU32(0x8218958Cu, f0_word == 0x3FC00000u ?
                    0xC0100000u : 0x80000000u);
    memory.WriteU32(0x82000C84u, 0x3F800001u);
    memory.WriteU32(0x821894F8u, 0x40100000u);
    memory.WriteU32(0x82189400u, 0xBF800000u);
    memory.WriteU32(0x83235AE4u, 0x11223344u);
    memory.WriteU32(0x83235AE8u, 0x55667788u);
    memory.WriteU32(Object + 60u, 0x00654321u);
}

bool SameMemory(const Window& first, const Window& second)
{
    for (GuestAddress page : Pages)
        if (std::memcmp(first.bytes + page, second.bytes + page, 0x1000))
            return false;
    return true;
}

PPCContext Seed(GuestAddress object, std::uint32_t host_csr)
{
    PPCContext ctx{};
    ctx.r3.u64 = 0xABCDEF0000000000ull | object;
    ctx.r10.u64 = 0x1111222233334444ull;
    ctx.r11.u64 = 0x5555666677778888ull;
    ctx.f0.f64 = 42.125;
    ctx.f13.f64 = -17.5;
    ctx.lr = 0x1234567887654321ull;
    ctx.fpscr.csr = host_csr | PPCFPSCRRegister::FlushMask;
    return ctx;
}

bool Compare(const Entry& entry, std::uint32_t f0_word, bool alias,
    Window& original, Window& recovered)
{
    HostFpGuard host;
    Initialize(original, f0_word);
    Initialize(recovered, f0_word);
    const GuestAddress object = alias ? AliasObject : Object;
    PPCContext raw = Seed(object, host.saved);
    PPCContext restored = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    entry.original(raw, original.bytes);
    host.control.setcsr(host.saved);

    GuestMemory memory(0, {recovered.bytes, Space});
    restored.fpscr.setcsr(restored.fpscr.csr);
    restored.fpscr.disableFlushMode();
    initializer::Effects effects{};
    if (!initializer::Apply(entry.address, memory, restored.r3.u64, effects) ||
        !effects.disable_flush_mode)
        throw std::runtime_error("initializer semantic mapping failed");
    restored.r10.u64 = effects.r10;
    restored.r11.u64 = effects.r11;
    restored.f0.f64 = effects.f0;
    restored.f13.f64 = effects.f13;
    const bool same = std::memcmp(&raw, &restored, sizeof(PPCContext)) == 0 &&
                      SameMemory(original, recovered);
#ifdef LO_METADATA_SNAN_ONLY
    GuestMemory raw_memory(0, {original.bytes, Space});
    const auto raw_word = raw_memory.ReadU32(Object + 64u);
    const auto recovered_word = memory.ReadU32(Object + 64u);
    std::printf("REPRO signaling-NaN original store %08x recovered store %08x; "
                "f0 %016llx/%016llx FPSCR %08x/%08x source 82000e50 object 00010000\n",
                raw_word, recovered_word,
                static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(raw.f0.f64)),
                static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(restored.f0.f64)),
                raw.fpscr.csr, restored.fpscr.csr);
    return !same && raw_word == 0x7F800001u &&
           recovered_word == 0x7FC00001u &&
           std::memcmp(&raw, &restored, sizeof(PPCContext)) == 0 &&
           raw.fpscr.csr == restored.fpscr.csr;
#endif
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL metadata initializer %08x edge %u alias %u r10 %llx/%llx r11 %llx/%llx f0 %llx/%llx f13 %llx/%llx\n",
            entry.address, f0_word, alias,
            static_cast<unsigned long long>(raw.r10.u64),
            static_cast<unsigned long long>(restored.r10.u64),
            static_cast<unsigned long long>(raw.r11.u64),
            static_cast<unsigned long long>(restored.r11.u64),
            static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(raw.f0.f64)),
            static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(restored.f0.f64)),
            static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(raw.f13.f64)),
            static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(restored.f13.f64)));
        const auto* first = reinterpret_cast<const std::uint8_t*>(&raw);
        const auto* second = reinterpret_cast<const std::uint8_t*>(&restored);
        for (std::size_t index = 0; index < sizeof(PPCContext); ++index)
            if (first[index] != second[index])
            {
                std::fprintf(stderr, "context byte %zu: %02x/%02x\n",
                             index, first[index], second[index]);
                break;
            }
        for (GuestAddress page : Pages)
            for (GuestAddress offset = 0; offset < 0x1000u; ++offset)
                if (original.bytes[page + offset] != recovered.bytes[page + offset])
                {
                    std::fprintf(stderr, "guest byte %08x: %02x/%02x\n", page + offset,
                        original.bytes[page + offset], recovered.bytes[page + offset]);
                    return false;
                }
    }
    return same;
}
} // namespace

int main()
{
    try
    {
        Window original, recovered;
#ifdef LO_METADATA_SNAN_ONLY
        if (!Compare(Entries[0], 0x7F800001u, false, original, recovered))
            return 1;
        std::puts("KNOWN MISMATCH: optimized generated C++ retains signaling-NaN store bits; independent float narrowing quiets it");
        return 0;
#else
        unsigned comparisons = 0;
        for (const Entry& entry : Entries)
        {
            if (!Compare(entry, 0x3FC00000u, false, original, recovered)) return 1;
            ++comparisons;
        }
        if (!Compare(Entries[0], 0x00000001u, false, original, recovered) ||
            !Compare(Entries[2], 0x3FC00000u, true, original, recovered)) return 1;
        comparisons += 2;

        GuestMemory memory(0, {recovered.bytes, Space});
        initializer::Effects untouched{1, 2, 3.0, 4.0, false};
        if (initializer::Apply(0xFFFFFFFFu, memory, Object, untouched) ||
            untouched.r10 != 1 || untouched.r11 != 2 ||
            untouched.f0 != 3.0 || untouched.f13 != 4.0 ||
            untouched.disable_flush_mode)
            throw std::runtime_error("unknown initializer changed effects");
        std::printf("PASS registered-metadata-initializers %u PPC comparisons + unknown mapping\n",
                    comparisons);
        std::puts("LIMIT bounded PPCContext and committed guest pages; FPSCR host flush applied by test adapter; signaling-NaN payload, runtime wrapper, and game scene unverified");
        return 0;
#endif
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
