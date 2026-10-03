#include "lo_semantics/manager_index_operations.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace operations = lo::semantic::gpu::manager_index_operations;
namespace tables = lo::semantic::gpu::manager_index_tables;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x30000u;
constexpr GuestAddress Entries = 0x40000u;
constexpr GuestAddress OldTable = 0x50000u;
constexpr GuestAddress NewTable = 0x60000u;
constexpr GuestAddress Replacement = 0x70000u;
constexpr GuestAddress Stack = 0x80080u;
constexpr GuestAddress Manager = 0x90000u;
constexpr GuestAddress Vtable = 0x90100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocMethod = 0x82345678u;
constexpr GuestAddress ReleaseMethod = 0x82345688u;
constexpr GuestAddress ResizeMethod = 0x82345680u;

enum class Mode { FloatMiss, FloatHit, FloatResize, FloatGrowth,
                  FloatFrameAlias, WordMiss, WordHit, WordNoTable };
struct Case
{
    GuestAddress address;
    PPCFunc* original;
    Mode mode;
};
const Case Cases[] = {
    {0x82326978u, __imp__sub_82326978, Mode::FloatMiss},
    {0x82326978u, __imp__sub_82326978, Mode::FloatHit},
    {0x82326978u, __imp__sub_82326978, Mode::FloatResize},
    {0x82326978u, __imp__sub_82326978, Mode::FloatGrowth},
    {0x82326978u, __imp__sub_82326978, Mode::FloatFrameAlias},
    {0x82713cf8u, __imp__sub_82713CF8, Mode::WordMiss},
    {0x82713cf8u, __imp__sub_82713CF8, Mode::WordHit},
    {0x82713cf8u, __imp__sub_82713CF8, Mode::WordNoTable},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0xa0000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("reserve manager-index operation guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct FpGuard
{
    PPCFPSCRRegister control{};
    std::uint32_t saved = control.getcsr();
    ~FpGuard() { control.setcsr(saved); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices, ArrayResizeServices,
                        operations::FpServices
{
    GuestMemory memory;
    PPCContext& context;
    Mode mode;
    std::vector<Event> events;
    unsigned fp_calls{};
    Services(std::uint8_t* bytes, PPCContext& cpu, Mode selected)
        : memory(0, {bytes, Space}), context(cpu), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    void InitializeManager() override
    { throw std::runtime_error("unexpected array manager initialization"); }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t storage) override
    {
        events.push_back({'D', {method, manager, storage, 0}});
        if (method != ReleaseMethod || manager != Manager)
            throw std::runtime_error("wrong release callback");
        return 0xdeadbeef00000001ull;
    }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        if (method != AllocMethod || manager != Manager || alignment != 8)
            throw std::runtime_error("wrong allocation callback");
        return 0x1234567800060000ull;
    }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'R', {method, manager, old_storage,
            (std::uint64_t{bytes} << 32) | argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8)
            throw std::runtime_error("wrong resize callback");
        if (mode == Mode::FloatResize)
            memory.WriteU32(Object + 16u, 2u);
        return Replacement;
    }
    void DisableFlushMode() override
    {
        ++fp_calls;
        context.fpscr.disableFlushMode();
    }
};

Services* active = nullptr;

void Initialize(Window& window, const Case& test)
{
    std::memset(window.bytes, 0xbd, 0xa0000);
    std::memset(window.bytes + 0x8330b000u, 0xbd, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 4u, AllocMethod | 3u);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Vtable + 12u, ReleaseMethod | 3u);
    const bool hit = test.mode == Mode::FloatHit || test.mode == Mode::WordHit;
    memory.WriteU32(Object, test.mode == Mode::FloatFrameAlias ?
        Stack - 52u : Entries);
    memory.WriteU32(Object + 4u, test.mode == Mode::FloatGrowth ? 10u :
        (hit ? 1u : 0u));
    memory.WriteU32(Object + 8u,
        test.mode == Mode::FloatResize ? 0u : 20u);
    memory.WriteU32(Object + 12u,
        test.mode == Mode::WordNoTable ? 0u : OldTable);
    memory.WriteU32(Object + 16u,
        test.mode == Mode::FloatGrowth ? 1u : 4u);
    for (GuestAddress i = 0; i < 32u; i += 4u)
        memory.WriteU32(OldTable + i, UINT32_MAX);
    for (GuestAddress i = 0; i < 11u; ++i)
    {
        memory.WriteU32(Entries + i * 16u + 4u, i + 1u);
        memory.WriteU32(Entries + i * 16u + 8u, i * 7u);
    }
    if (hit)
    {
        memory.WriteU32(OldTable, 0u);
        memory.WriteU32(Entries, UINT32_MAX);
        memory.WriteU32(Entries + 4u, 0x12345678u);
        memory.WriteU32(Entries + 8u, 5u);
    }
}

bool SameMemory(const Window& a, const Window& b)
{
    return std::memcmp(a.bytes, b.bytes, 0xa0000) == 0 &&
        std::memcmp(a.bytes + 0x8330b000u,
                    b.bytes + 0x8330b000u, 0x1000) == 0;
}

bool Compare(const Case& test)
{
    FpGuard guard;
    Window original, recovered;
    Initialize(original, test);
    Initialize(recovered, test);
    PPCContext raw{};
    raw.r1.u64 = 0xfedcba9800000000ull | Stack;
    raw.r3.u64 = 0xabcdef1200000000ull | Object;
    raw.r4.u64 = 0x1234567800000005ull;
    raw.r5.u64 = 0x9999aaaabbbbccccull;
    raw.r28.u64 = 0x1111222233334444ull;
    raw.r29.u64 = 0x5555666677778888ull;
    raw.r30.u64 = 0x9999aaaabbbb0001ull;
    raw.r31.u64 = 0xdddd111122223333ull;
    raw.lr = 0x8765432182345678ull;
    raw.f1.f64 = -3.125;
    raw.f31.f64 = 9.25;
    raw.fpscr.csr = guard.saved | PPCFPSCRRegister::FlushMask;
    PPCContext restored = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    Services expected(original.bytes, raw, test.mode);
    active = &expected;
    test.original(raw, original.bytes);
    guard.control.setcsr(guard.saved);

    restored.fpscr.setcsr(restored.fpscr.csr);
    Services actual(recovered.bytes, restored, test.mode);
    operations::FrameRegisters frame{restored.lr,
        restored.r28.u64, restored.r29.u64, restored.r30.u64,
        restored.r31.u64, restored.f31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!operations::Apply(test.address, actual.memory, actual, actual,
            actual, restored.r3.u64, restored.r4.u64, restored.r5.u64,
            restored.f1.u64, restored.r1.u64, frame, result))
        throw std::runtime_error("manager-index operation not mapped");
    const bool floating = test.address == 0x82326978u;
    const unsigned expected_fp = floating ?
        (test.mode == Mode::FloatHit ? 2u : 3u) : 0u;
    const bool same = SameMemory(original, recovered) &&
        expected.events == actual.events && actual.fp_calls == expected_fp &&
        raw.r3.u64 == result && raw.r1.u64 == restored.r1.u64 &&
        raw.lr == frame.lr && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31 && raw.f31.u64 == frame.f31_bits &&
        raw.fpscr.csr == restored.fpscr.csr;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL manager-index-operation %08x mode=%u r3 %016llx/%016llx fp %u/%u events %zu/%zu\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result), actual.fp_calls,
            expected_fp, expected.events.size(), actual.events.size());
        for (std::size_t i = 0, shown = 0; i < 0xa0000 && shown < 8; ++i)
            if (original.bytes[i] != recovered.bytes[i])
            {
                std::fprintf(stderr, "  memory %08zx %02x/%02x\n", i,
                    original.bytes[i], recovered.bytes[i]);
                ++shown;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
PPC_FUNC(sub_82326B08)
{
    tables::FrameRegisters frame{ctx.lr, ctx.r28.u64, ctx.r29.u64,
        ctx.r30.u64, ctx.r31.u64};
    std::uint64_t result = 0;
    if (!tables::Apply(0x82326b08u, active->memory, *active, *active,
            ctx.r3.u64, ctx.r4.u64, ctx.r5.u64, ctx.r6.u64,
            ctx.r1.u64, frame, result))
        throw std::runtime_error("lower index rebuild unavailable");
    ctx.r3.u64 = result;
    ctx.lr = frame.lr;
    ctx.r28.u64 = frame.r28;
    ctx.r29.u64 = frame.r29;
    ctx.r30.u64 = frame.r30;
    ctx.r31.u64 = frame.r31;
}
PPC_FUNC(sub_8229F678)
{
    ResizeArray(active->memory, *active, ctx.r3.u32, ctx.r4.u32,
                ctx.r5.u32);
}

int main()
{
    try
    {
        for (const Case& test : Cases)
            if (!Compare(test)) return 1;
        Window window;
        PPCContext context{};
        Services services(window.bytes, context, Mode::WordMiss);
        std::array<std::uint8_t, 16> scratch{};
        GuestMemory memory(0, scratch);
        operations::FrameRegisters frame{1, 2, 3, 4, 5, 6};
        std::uint64_t result = 7;
        if (operations::Apply(0x82713d00u, memory, services, services,
                services, 8, 9, 10, 11, 12, frame, result) ||
            result != 7 || frame.lr != 1 || frame.f31_bits != 6 ||
            !services.events.empty() || services.fp_calls != 0 ||
            scratch != std::array<std::uint8_t, 16>{})
            return 1;
        std::printf("PASS manager-index-operations 8 original PPC cases + unknown\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "manager-index-operations oracle: %s\n",
                     error.what());
        return 2;
    }
}
