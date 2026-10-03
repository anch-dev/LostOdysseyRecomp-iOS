#include "lo_semantics/instance_manager_link_family.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace family = lo::semantic::gpu::instance_manager_link_family;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Stack = 0x30080u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress F0Source = 0x82000e50u;
constexpr GuestAddress F13Source = 0x8218958cu;
constexpr std::array<GuestAddress, 4> Pages = {
    0x10000u, 0x30000u, 0x82000000u, 0x82189000u,
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes)
            throw std::runtime_error("reserve manager-link guest space");
        for (GuestAddress page : Pages)
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit manager-link guest page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct FpGuard
{
    PPCFPSCRRegister control{};
    std::uint32_t saved = control.getcsr();
    ~FpGuard() { control.setcsr(saved); }
};

struct FpServices final : family::FpServices
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

void Initialize(Window& window)
{
    for (GuestAddress page : Pages)
        std::memset(window.bytes + page, 0xbd, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(F0Source, 0x3f400000u);
    memory.WriteU32(F13Source, 0xc0200000u);
}

bool SameMemory(const Window& a, const Window& b)
{
    for (GuestAddress page : Pages)
        if (std::memcmp(a.bytes + page, b.bytes + page, 0x1000))
            return false;
    return true;
}

PPCContext Seed(std::uint64_t full_object, std::uint64_t full_sp,
    std::uint32_t host_csr)
{
    PPCContext ctx{};
    ctx.r1.u64 = full_sp;
    ctx.r3.u64 = full_object;
    ctx.r4.u64 = 0xfeedbeef12345678ull;
    ctx.r9.u64 = 0x9911223344556677ull;
    ctx.r10.u64 = 0xaa11223344556677ull;
    ctx.r11.u64 = 0xbb11223344556677ull;
    ctx.r30.u64 = 0xcc11223344556677ull;
    ctx.r31.u64 = 0xdd11223344556677ull;
    ctx.f0.f64 = 7.5;
    ctx.f13.f64 = -8.25;
    ctx.lr = 0xee11223344556677ull;
    ctx.fpscr.csr = host_csr | PPCFPSCRRegister::FlushMask;
    return ctx;
}

bool Compare(GuestAddress address, PPCFunc* original,
    GuestAddress object, Window& expected, Window& actual)
{
    FpGuard guard;
    Initialize(expected);
    Initialize(actual);
    const std::uint64_t full_object = 0xabcdef1200000000ull | object;
    const std::uint64_t full_sp = 0xfedcba9800000000ull | Stack;
    PPCContext raw = Seed(full_object, full_sp, guard.saved);
    PPCContext restored = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    original(raw, expected.bytes);
    guard.control.setcsr(guard.saved);

    GuestMemory memory(0, {actual.bytes, Space});
    restored.fpscr.setcsr(restored.fpscr.csr);
    FpServices fp(restored);
    family::Registers registers{restored.lr, restored.r9.u64, restored.r10.u64,
        restored.r11.u64, restored.r30.u64, restored.r31.u64,
        restored.f0.f64, restored.f13.f64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(address, memory, fp, restored.r3.u64,
            restored.r4.u64, restored.r1.u64, registers, result))
        throw std::runtime_error("manager-link entry not mapped");
    restored.r3.u64 = result;
    restored.lr = registers.lr;
    restored.r9.u64 = registers.r9;
    restored.r10.u64 = registers.r10;
    restored.r11.u64 = registers.r11;
    restored.r30.u64 = registers.r30;
    restored.r31.u64 = registers.r31;
    restored.f0.f64 = registers.f0;
    restored.f13.f64 = registers.f13;
    const bool same = SameMemory(expected, actual) && fp.calls == 2u &&
        raw.r1.u64 == restored.r1.u64 && raw.r3.u64 == restored.r3.u64 &&
        raw.lr == restored.lr && raw.r9.u64 == restored.r9.u64 &&
        raw.r10.u64 == restored.r10.u64 && raw.r11.u64 == restored.r11.u64 &&
        raw.r30.u64 == restored.r30.u64 && raw.r31.u64 == restored.r31.u64 &&
        std::bit_cast<std::uint64_t>(raw.f0.f64) ==
            std::bit_cast<std::uint64_t>(restored.f0.f64) &&
        std::bit_cast<std::uint64_t>(raw.f13.f64) ==
            std::bit_cast<std::uint64_t>(restored.f13.f64) &&
        raw.fpscr.csr == restored.fpscr.csr;
    if (!same)
        std::fprintf(stderr, "FAIL manager-link %08x object %08x r3 %016llx/%016llx r9 %016llx/%016llx\n",
            address, object, static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(restored.r3.u64),
            static_cast<unsigned long long>(raw.r9.u64),
            static_cast<unsigned long long>(restored.r9.u64));
    return same;
}
} // namespace

PPC_FUNC(sub_82700FD8) { __imp__sub_82700FD8(ctx, base); }

int main()
{
    try
    {
        Window expected, actual;
        const struct Case { GuestAddress address; PPCFunc* original;
            GuestAddress object; } cases[] = {
            {0x82700fd8u, __imp__sub_82700FD8, Object},
            {0x82700f28u, __imp__sub_82700F28, Object},
            {0x82700f28u, __imp__sub_82700F28, F0Source - 40u},
            {0x82700f28u, __imp__sub_82700F28, F13Source - 40u},
            {0x82700f28u, __imp__sub_82700F28, Stack - 24u},
        };
        for (const Case& test : cases)
            if (!Compare(test.address, test.original, test.object,
                    expected, actual))
                return 1;

        std::array<std::uint8_t, 16> scratch{};
        GuestMemory memory(0, scratch);
        PPCContext ctx{};
        FpServices fp(ctx);
        family::Registers registers{1, 2, 3, 4, 5, 6, 7.0, 8.0};
        std::uint64_t result = 9;
        if (family::Apply(0x827010d0u, memory, fp, 10, 11, 12,
                registers, result) || fp.calls != 0 || result != 9 ||
            registers.lr != 1 || registers.r31 != 6 ||
            scratch != std::array<std::uint8_t, 16>{})
            return 1;
        std::printf("PASS instance-manager-link 5 original PPC cases + unknown\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "manager-link oracle: %s\n", error.what());
        return 2;
    }
}
