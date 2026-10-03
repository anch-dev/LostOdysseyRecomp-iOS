#include "lo_semantics/instance_component_initializer_family.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace component = lo::semantic::gpu::instance_component_initializer_family;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress AliasObject = 0x8218958cu - 96u;
constexpr std::array<GuestAddress, 3> Pages = {
    0x10000u, 0x82000000u, 0x82189000u,
};
/* IMAGE_WORDS */

struct Entry
{
    GuestAddress address;
    GuestAddress vtable;
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
            throw std::runtime_error("reserve component guest space");
        for (GuestAddress page : Pages)
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit component guest page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct HostFpGuard
{
    PPCFPSCRRegister control{};
    std::uint32_t saved = control.getcsr();
    ~HostFpGuard() { control.setcsr(saved); }
};

struct FpServices final : component::ComponentFpServices
{
    PPCContext& context;
    unsigned calls{};
    explicit FpServices(PPCContext& value) : context(value) {}
    void DisableFlushMode() override
    {
        ++calls;
        context.fpscr.disableFlushMode();
    }
};

void Initialize(Window& window, bool alternate)
{
    for (GuestAddress page : Pages)
        std::memset(window.bytes + page, 0xbd, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(0x8218958cu,
        alternate ? 0xc0100000u : ImageF13Word);
    memory.WriteU32(0x82000e50u,
        alternate ? 0x3fc00000u : ImageF0Word);
}

PPCContext Seed(std::uint64_t r3, std::uint32_t host_csr)
{
    PPCContext context{};
    context.r3.u64 = r3;
    context.r10.u64 = 0x1111222233334444ull;
    context.r11.u64 = 0x5555666677778888ull;
    context.f0.f64 = 42.125;
    context.f13.f64 = -17.5;
    context.lr = 0x1234567887654321ull;
    context.fpscr.csr = host_csr | PPCFPSCRRegister::FlushMask;
    return context;
}

bool SameMemory(const Window& original, const Window& recovered)
{
    for (GuestAddress page : Pages)
        if (std::memcmp(original.bytes + page, recovered.bytes + page, 0x1000))
            return false;
    return true;
}

bool Compare(const Entry& entry, std::uint64_t incoming_r3,
    bool alternate, Window& original, Window& recovered)
{
    HostFpGuard host;
    Initialize(original, alternate);
    Initialize(recovered, alternate);
    PPCContext raw = Seed(incoming_r3, host.saved);
    PPCContext restored = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    entry.original(raw, original.bytes);
    host.control.setcsr(host.saved);

    GuestMemory memory(0, {recovered.bytes, Space});
    restored.fpscr.setcsr(restored.fpscr.csr);
    FpServices services(restored);
    component::ComponentEffects effects{
        0xaaaa111122223333ull, 0xbbbb444455556666ull, 9.25, -8.5};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!component::Apply(entry.address, memory, services, incoming_r3,
            effects, result))
        throw std::runtime_error("missing component initializer");
    const bool nonnull = static_cast<GuestAddress>(incoming_r3) != 0;
    if (nonnull)
    {
        restored.r10.u64 = effects.r10;
        restored.r11.u64 = effects.r11;
        restored.f0.f64 = effects.f0;
        restored.f13.f64 = effects.f13;
    }
    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    const bool unchanged_null_effects = nonnull ||
        (effects.r10 == 0xaaaa111122223333ull &&
         effects.r11 == 0xbbbb444455556666ull &&
         effects.f0 == 9.25 && effects.f13 == -8.5);
    const bool same = result == raw.r3.u64 &&
        services.calls == (nonnull ? 1u : 0u) &&
        unchanged_null_effects &&
        restored.fpscr.csr == raw.fpscr.csr &&
        restored.r10.u64 == raw.r10.u64 &&
        restored.r11.u64 == raw.r11.u64 &&
        std::bit_cast<std::uint64_t>(restored.f0.f64) ==
            std::bit_cast<std::uint64_t>(raw.f0.f64) &&
        std::bit_cast<std::uint64_t>(restored.f13.f64) ==
            std::bit_cast<std::uint64_t>(raw.f13.f64) &&
        restored.lr == raw.lr && SameMemory(original, recovered) &&
        (!nonnull || memory.ReadU32(object) == entry.vtable);
    if (!same)
        std::fprintf(stderr,
            "FAIL component %08x r3 %016llx alt %u calls %u\n",
            entry.address, static_cast<unsigned long long>(incoming_r3),
            alternate, services.calls);
    return same;
}
} // namespace

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (const Entry& entry : Entries)
        {
            if (!Compare(entry, 0xabcdef0000010000ull, false,
                    original, recovered))
                return 1;
            ++cases;
        }
        if (!Compare(Entries[0], 0xabcdef0000010000ull, true,
                original, recovered) ||
            !Compare(Entries[0], 0xabcdef0000000000ull, false,
                original, recovered) ||
            !Compare(Entries[0], 0xabcdef0000000000ull | AliasObject, false,
                original, recovered))
            return 1;
        cases += 3;

        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        PPCContext context{};
        FpServices services(context);
        component::ComponentEffects effects{};
        std::uint64_t result = 0x1234567800000000ull;
        if (component::Apply(0xffffffffu, memory, services, 0x100u,
                effects, result) || services.calls != 0 ||
            result != 0x1234567800000000ull ||
            bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown address changed state");
        ++cases;
        std::printf("PASS instance-component %zu representative entries %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT finite inputs only; signaling-NaN stores, full CR state, and 64-bit fault/MMIO width remain unverified");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
