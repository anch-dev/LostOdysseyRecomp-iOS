#include "lo_semantics/registered_metadata_initializers.h"
#include "lo_semantics/loaded_single.h"

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
using lo::semantic::gpu::LoadedSingle;

// Fixed format vectors from the documented exponent/fraction mapping, rather
// than host arithmetic casts (which quiet signaling NaNs).
static_assert(LoadedSingle::FromWord(0x3f800000u).FprBits() == 0x3ff0000000000000ull);
static_assert(LoadedSingle::FromWord(0x00000001u).FprBits() == 0x36a0000000000000ull);
static_assert(LoadedSingle::FromWord(0x007fffffu).FprBits() == 0x380fffffc0000000ull);
static_assert(LoadedSingle::FromWord(0x80000000u).FprBits() == 0x8000000000000000ull);
static_assert(LoadedSingle::FromWord(0xff800000u).FprBits() == 0xfff0000000000000ull);
static_assert(LoadedSingle::FromWord(0x7fc12345u).FprBits() == 0x7ff82468a0000000ull);
static_assert(LoadedSingle::FromWord(0x7f800001u).FprBits() == 0x7ff0000020000000ull);
static_assert(LoadedSingle::FromWord(0x7f800001u).StoreWord() == 0x7f800001u);

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

struct FpAdapter final : initializer::FpServices
{
    PPCContext& context;
    GuestMemory& memory;
    initializer::Effects& effects;
    GuestAddress address;
    GuestAddress object;
    unsigned calls{};

    FpAdapter(PPCContext& context_, GuestMemory& memory_,
        initializer::Effects& effects_, GuestAddress address_, GuestAddress object_)
        : context(context_), memory(memory_), effects(effects_),
          address(address_), object(object_) {}

    void DisableFlushMode() override
    {
        if (address == 0x825A8448u &&
            effects.r10 != (memory.ReadU32(object + 60u) | 0x80000000u))
            throw std::runtime_error("flush preceded metadata flag load");
        if (address == 0x8270BBE8u && memory.ReadU32(object + 80u) != 1u)
            throw std::runtime_error("flush preceded metadata flag store");
        ++calls;
        context.fpscr.disableFlushMode();
    }
};

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
    initializer::Effects effects{};
    FpAdapter fp(restored, memory, effects, entry.address, object);
    if (!initializer::Apply(entry.address, memory, fp, restored.r3.u64, effects) ||
        !effects.disable_flush_mode || fp.calls != 1)
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
    const bool expected_gap = !same && raw_word == 0x7F800001u &&
        recovered_word == 0x7F800001u && SameMemory(original, recovered) &&
        std::bit_cast<std::uint64_t>(raw.f0.f64) == 0x7FF8000020000000ull &&
        std::bit_cast<std::uint64_t>(restored.f0.f64) == 0x7FF0000020000000ull &&
        raw.fpscr.csr == restored.fpscr.csr;
    raw.f0.u64 = restored.f0.u64;
    return expected_gap && std::memcmp(&raw, &restored, sizeof(PPCContext)) == 0;
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
        std::puts("KNOWN MISMATCH: generated C++ quiets the signaling-NaN FPR; documented PPC format mapping retains signaling bits; /O2 store words agree");
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
        PPCContext unknown_context{};
        FpAdapter unknown_fp(unknown_context, memory, untouched, 0xFFFFFFFFu, Object);
        if (initializer::Apply(0xFFFFFFFFu, memory, unknown_fp, Object, untouched) ||
            untouched.r10 != 1 || untouched.r11 != 2 ||
            untouched.f0 != 3.0 || untouched.f13 != 4.0 ||
            untouched.disable_flush_mode || unknown_fp.calls != 0)
            throw std::runtime_error("unknown initializer changed effects");
        std::printf("PASS registered-metadata-initializers %u PPC comparisons + unknown mapping\n",
                    comparisons);
        std::puts("PASS seven documented format vectors + signaling-NaN unchanged store; flush callback order and unknown no-call checked");
        std::puts("LIMIT bounded PPCContext and committed guest pages; signaling-NaN differs from generated C++ FPR; runtime wrapper, Xenon hardware and game scene unverified");
        return 0;
#endif
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
