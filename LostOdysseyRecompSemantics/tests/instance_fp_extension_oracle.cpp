#include "lo_semantics/instance_fp_extension_family.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace extension = lo::semantic::gpu::instance_fp_extension_family;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress F13Source = 0x8218958cu;
constexpr GuestAddress F0Source = 0x82000e50u;
constexpr std::array<GuestAddress, 4> Pages = {
    Object, 0x82000000u, 0x82001000u, 0x82189000u,
};
/* IMAGE_WORDS */

struct Entry { GuestAddress address; PPCFunc* original; };
const Entry Entries[] = {
/* ENTRY_TABLE */
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve FP extension guest space");
        for (GuestAddress page : Pages)
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit FP extension page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct HostFpGuard
{
    PPCFPSCRRegister control{};
    std::uint32_t saved = control.getcsr();
    ~HostFpGuard() { control.setcsr(saved); }
};

struct FpServices final : instance_component_initializer_family::ComponentFpServices
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

void Initialize(Window& window, bool alternate, bool clear_flag)
{
    for (GuestAddress page : Pages)
        std::memset(window.bytes + page, 0xbd, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(F13Source, alternate ? 0xc0100000u : ImageF13Word);
    memory.WriteU32(F0Source, alternate ? 0x3fc00000u : ImageF0Word);
    if (clear_flag)
        memory.WriteU32(Object + 60u, 0u);
}

PPCContext Seed(std::uint64_t r3, std::uint32_t host_csr)
{
    PPCContext ctx{};
    ctx.r3.u64 = r3;
    ctx.f0.f64 = 42.125;
    ctx.f13.f64 = -17.5;
    ctx.lr = 0x1234567887654321ull;
    ctx.fpscr.csr = host_csr | PPCFPSCRRegister::FlushMask;
    return ctx;
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
    const bool force = entry.address == 0x82730310u;
    Initialize(original, alternate, force);
    Initialize(recovered, alternate, force);
    PPCContext raw = Seed(incoming_r3, host.saved);
    PPCContext semantic = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    entry.original(raw, original.bytes);
    host.control.setcsr(host.saved);

    GuestMemory memory(0, {recovered.bytes, Space});
    semantic.fpscr.setcsr(semantic.fpscr.csr);
    FpServices services(semantic);
    extension::FpEffects effects{semantic.f0.f64, semantic.f13.f64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!extension::Apply(entry.address, memory, services, incoming_r3,
                          effects, result))
        throw std::runtime_error("missing FP extension initializer");
    const bool nonnull = static_cast<GuestAddress>(incoming_r3) != 0;
    const bool same = result == raw.r3.u64 &&
        services.calls == (nonnull ? 1u : 0u) &&
        semantic.fpscr.csr == raw.fpscr.csr &&
        std::bit_cast<std::uint64_t>(effects.f0) ==
            std::bit_cast<std::uint64_t>(raw.f0.f64) &&
        std::bit_cast<std::uint64_t>(effects.f13) ==
            std::bit_cast<std::uint64_t>(raw.f13.f64) &&
        SameMemory(original, recovered);
    if (!same)
        std::fprintf(stderr,
            "FAIL FP extension %08x r3 %016llx alternate %u callbacks %u f0 %016llx/%016llx\n",
            entry.address, static_cast<unsigned long long>(incoming_r3),
            alternate, services.calls,
            static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(effects.f0)),
            static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(raw.f0.f64)));
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
                         original, recovered)) return 1;
            ++cases;
        }
        const Entry* primitive = nullptr;
        for (const Entry& entry : Entries)
            if (entry.address == 0x825e09b8u) primitive = &entry;
        if (!primitive ||
            !Compare(*primitive, 0xabcdef0000000000ull | F0Source,
                     false, original, recovered) ||
            !Compare(Entries[0], 0xabcdef0000010000ull,
                     true, original, recovered) ||
            !Compare(Entries[0], 0xabcdef0000000000ull,
                     false, original, recovered))
            return 1;
        cases += 3;

        std::array<std::uint8_t, 16> bytes{};
        GuestMemory memory(0, bytes);
        PPCContext ctx{};
        FpServices services(ctx);
        extension::FpEffects effects{42.125, -17.5};
        std::uint64_t result = 0x1234567800000000ull;
        if (extension::Apply(0xffffffffu, memory, services, 0x100u,
                             effects, result) || services.calls != 0 ||
            result != 0x1234567800000000ull ||
            effects.f0 != 42.125 || effects.f13 != -17.5 ||
            bytes != std::array<std::uint8_t, 16>{})
            throw std::runtime_error("unknown FP extension changed state");
        ++cases;
        std::printf("PASS instance-FP-extension %u bounded PPC comparisons\n", cases);
        std::puts("LIMIT finite words; generated-C++ sNaN FPR gap, volatile GPR/CR and U64 fault/MMIO width excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
