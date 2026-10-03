#include "lo_semantics/instance_connection_initializer_family.h"

#include "lo_semantics/instance_linker_composed_family.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/string_property_initializer.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace connection = lo::semantic::gpu::instance_connection_initializer_family;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Storage = 0x20000u;
constexpr GuestAddress Manager = 0x25000u;
constexpr GuestAddress Vtable = 0x25100u;
constexpr GuestAddress Stack = 0x30000u;
constexpr GuestAddress ManagerGlobal = 0x8330B608u;
constexpr GuestAddress Method = 0x82345680u;

enum class Mode { BitEmpty, BitResize, BitWrap, BitFrameAlias, Net, Child, Tcpip,
    TailNull, ChildNull, TcpipNull };

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve connection guest window");
        for (auto [page, size] : {std::pair{0u, 0x40000u},
             std::pair{0x82000000u, 0x1000u},
             std::pair{0x82189000u, 0x1000u},
             std::pair{0x82218000u, 0x1000u},
             std::pair{0x83235000u, 0x1000u},
             std::pair{0x8330B000u, 0x1000u},
             std::pair{0x83318000u, 0x1000u},
             std::pair{0x8336A000u, 0x1000u}})
            if (!VirtualAlloc(bytes + page, size, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit connection guest page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    std::array<std::uint64_t, 5> values;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices, ManagerFacadeServices,
    instance_component_initializer_family::ComponentFpServices
{
    GuestMemory memory;
    Mode mode;
    PPCContext* context{};
    std::vector<Event> events;
    unsigned fp_calls{};
    Services(std::uint8_t* bytes, Mode selected, PPCContext* fp)
        : memory(0, {bytes, Space}), mode(selected), context(fp) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({{method, manager, old_storage, bytes, argument}});
        if (method != Method || manager != Manager || argument != 8u)
            throw std::runtime_error("unexpected resize call");
        if (mode == Mode::BitResize)
            memory.WriteU32(Object + 116u, 1u);
        return Storage;
    }
    void DisableFlushMode() override
    {
        ++fp_calls;
        context->fpscr.disableFlushMode();
    }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected allocator call"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected release"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected allocation"); }
};

Services* active = nullptr;

void Setup(Window& window)
{
    std::memset(window.bytes, 0, 0x40000);
    for (GuestAddress page : {0x82000000u, 0x82189000u, 0x82218000u,
         0x83235000u, 0x8330B000u, 0x83318000u, 0x8336A000u})
        std::memset(window.bytes + page, 0, 0x1000);
    GuestMemory memory(0, {window.bytes, Space});
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, Method | 3u);
    memory.WriteU32(0x83235AA4u, 0x11111111u);
    memory.WriteU32(0x83235AA0u, 0x22222222u);
    memory.WriteU32(0x83235AACu, 0x33333333u);
    memory.WriteU32(0x833180ACu, 0x44444444u);
    memory.WriteU32(0x82000FE8u, 0x3FF00000u);
    memory.WriteU32(0x82000FECu, 0);
    memory.WriteU32(0x8218958Cu, 0x3F800000u);
    memory.WriteU32(0x822184DCu, 0x40000000u);
    // Four property source headers are empty; this bounded case exercises
    // composition and field order without an allocator/release callback.
    for (GuestAddress header : {0x8336A894u, 0x8336A8D0u,
         0x8336A8ACu, 0x8336A8DCu})
        for (GuestAddress offset : {0u, 4u, 8u})
            memory.WriteU32(header + offset, 0);
    memory.WriteU32(Object + 8u, 0x01234567u);
}

struct HostFpGuard
{
    PPCFPSCRRegister control{};
    std::uint32_t saved = control.getcsr();
    ~HostFpGuard() { control.setcsr(saved); }
};

PPCContext Seed(Mode mode, std::uint32_t csr)
{
    PPCContext ctx{};
    ctx.r1.u64 = mode == Mode::Net ?
        0x1234567800030000ull : Stack;
    ctx.lr = 0xDEADBEEF12345678ull;
    for (unsigned number = 25; number <= 31; ++number)
    {
        PPCRegister* registers[] = {&ctx.r25, &ctx.r26, &ctx.r27,
            &ctx.r28, &ctx.r29, &ctx.r30, &ctx.r31};
        registers[number - 25]->u64 = 0x1111000000000000ull + number;
    }
    const bool null_object = mode == Mode::TailNull ||
        mode == Mode::ChildNull || mode == Mode::TcpipNull;
    ctx.r3.u64 = 0xABCDEF0000000000ull |
        (null_object ? 0u : mode == Mode::BitFrameAlias ? Stack - 96u : Object);
    ctx.r4.u64 = mode == Mode::BitResize ? 16u :
        mode == Mode::BitWrap ? 0xDEADBEEFFFFFFFFFull : 0u;
    ctx.f0.f64 = 42.125;
    ctx.f13.f64 = -17.5;
    ctx.fpscr.csr = csr | PPCFPSCRRegister::FlushMask;
    return ctx;
}

bool SameMemory(const Window& expected, const Window& actual)
{
    if (std::memcmp(expected.bytes, actual.bytes, 0x40000)) return false;
    for (GuestAddress page : {0x82000000u, 0x82189000u, 0x82218000u,
         0x83235000u, 0x8330B000u, 0x83318000u, 0x8336A000u})
        if (std::memcmp(expected.bytes + page, actual.bytes + page, 0x1000))
            return false;
    return true;
}

bool Compare(Mode mode, Window& expected_window, Window& actual_window)
{
    HostFpGuard host;
    Setup(expected_window);
    Setup(actual_window);
    PPCContext raw = Seed(mode, host.saved);
    PPCContext actual = raw;
    Services expected(expected_window.bytes, mode, &raw);
    Services semantic(actual_window.bytes, mode, &actual);
    const GuestAddress address = mode == Mode::BitEmpty ||
        mode == Mode::BitResize || mode == Mode::BitWrap ||
        mode == Mode::BitFrameAlias ? 0x82752768u :
        mode == Mode::Net ? 0x8267A1F0u : mode == Mode::Child ||
        mode == Mode::ChildNull ? 0x8267A040u : mode == Mode::Tcpip ||
        mode == Mode::TcpipNull ? 0x8272CBD8u : 0x82679F50u;
    active = &expected;
    raw.fpscr.setcsr(raw.fpscr.csr);
    switch (address)
    {
    case 0x82752768u: __imp__sub_82752768(raw, expected_window.bytes); break;
    case 0x8267A1F0u: __imp__sub_8267A1F0(raw, expected_window.bytes); break;
    case 0x8267A040u: __imp__sub_8267A040(raw, expected_window.bytes); break;
    case 0x8272CBD8u: __imp__sub_8272CBD8(raw, expected_window.bytes); break;
    default: __imp__sub_82679F50(raw, expected_window.bytes); break;
    }
    host.control.setcsr(host.saved);

    connection::FrameRegisters frame{actual.lr, actual.r25.u64,
        actual.r26.u64, actual.r27.u64, actual.r28.u64,
        actual.r29.u64, actual.r30.u64, actual.r31.u64};
    connection::FpEffects effects{actual.f0.f64, actual.f13.f64};
    std::uint64_t result = 0xCAFEBABEDEADBEEFull;
    if (!connection::Apply(address, semantic.memory, semantic, semantic,
            semantic, actual.r3.u64, actual.r4.u64, actual.r1.u64,
            frame, effects, result))
        throw std::runtime_error("missing connection mapping");
    actual.r3.u64 = result;
    actual.lr = frame.lr;
    actual.r25.u64 = frame.r25;
    actual.r26.u64 = frame.r26;
    actual.r27.u64 = frame.r27;
    actual.r28.u64 = frame.r28;
    actual.r29.u64 = frame.r29;
    actual.r30.u64 = frame.r30;
    actual.r31.u64 = frame.r31;
    actual.f0.f64 = effects.f0;
    actual.f13.f64 = effects.f13;
    const bool same = raw.r3.u64 == actual.r3.u64 &&
        raw.r1.u64 == actual.r1.u64 && raw.lr == actual.lr &&
        raw.r25.u64 == actual.r25.u64 && raw.r26.u64 == actual.r26.u64 &&
        raw.r27.u64 == actual.r27.u64 && raw.r28.u64 == actual.r28.u64 &&
        raw.r29.u64 == actual.r29.u64 && raw.r30.u64 == actual.r30.u64 &&
        raw.r31.u64 == actual.r31.u64 &&
        std::bit_cast<std::uint64_t>(raw.f0.f64) ==
            std::bit_cast<std::uint64_t>(actual.f0.f64) &&
        std::bit_cast<std::uint64_t>(raw.f13.f64) ==
            std::bit_cast<std::uint64_t>(actual.f13.f64) &&
        raw.fpscr.csr == actual.fpscr.csr &&
        expected.events == semantic.events && SameMemory(expected_window,
                                                           actual_window);
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL connection mode=%u r3=%llx/%llx lr=%llx/%llx events=%zu/%zu\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(actual.r3.u64),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(actual.lr),
            expected.events.size(), semantic.events.size());
        for (GuestAddress offset = 0; offset < 0x40000u; ++offset)
            if (expected_window.bytes[offset] != actual_window.bytes[offset])
            {
                std::fprintf(stderr, "  first byte %08x = %02x/%02x\n",
                    offset, expected_window.bytes[offset],
                    actual_window.bytes[offset]);
                break;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_25) { __imp____savegprlr_25(ctx, base); }
PPC_FUNC(__restgprlr_25) { __imp____restgprlr_25(ctx, base); }
PPC_FUNC(__savegprlr_29) { __imp____savegprlr_29(ctx, base); }
PPC_FUNC(__restgprlr_29) { __imp____restgprlr_29(ctx, base); }
PPC_FUNC(sub_8267A1F0) { __imp__sub_8267A1F0(ctx, base); }
PPC_FUNC(sub_82752768) { __imp__sub_82752768(ctx, base); }
PPC_FUNC(sub_823F34B8)
{
    instance_linker_composed_family::FrameRegisters frame{
        ctx.lr, ctx.r30.u64, ctx.r31.u64};
    std::uint64_t result = 0;
    if (!instance_linker_composed_family::Apply(0x823F34B8u,
            active->memory, *active, ctx.r3.u64, ctx.r1.u32,
            frame, result))
        throw std::runtime_error("missing archive dependency");
    ctx.r3.u64 = result;
}
PPC_FUNC(sub_82496948)
{
    string_property_initializer::FrameRegisters frame{ctx.lr, ctx.r28.u64,
        ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
    std::uint64_t result = 0;
    if (!string_property_initializer::Apply(0x82496948u,
            active->memory, *active, *active, ctx.r3.u64, ctx.r4.u64,
            ctx.r1.u64, frame, result))
        throw std::runtime_error("missing property dependency");
    ctx.r3.u64 = result;
    ctx.lr = frame.lr;
    ctx.r28.u64 = frame.r28;
    ctx.r29.u64 = frame.r29;
    ctx.r30.u64 = frame.r30;
    ctx.r31.u64 = frame.r31;
}
PPC_FUNC(sub_8229F678)
{ ResizeArray(active->memory, *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); }
PPC_FUNC(sub_82B7BC40)
{ (void)FillGuestMemory(active->memory, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); }

int main()
{
    try
    {
        Window expected, actual;
        unsigned cases = 0;
        for (Mode mode : {Mode::BitEmpty, Mode::BitResize, Mode::BitWrap,
             Mode::BitFrameAlias,
             Mode::Net, Mode::Child, Mode::Tcpip,
             Mode::TailNull, Mode::ChildNull, Mode::TcpipNull})
        {
            if (!Compare(mode, expected, actual)) return 1;
            ++cases;
        }
        Services services(actual.bytes, Mode::BitEmpty, nullptr);
        connection::FrameRegisters frame{0x11223344u, 1, 2, 3, 4, 5, 6, 7};
        connection::FpEffects effects{1.25, 2.5};
        std::uint64_t result = 0xFEDCBA9876543210ull;
        if (connection::Apply(0xFFFFFFFFu, services.memory, services,
                services, services, Object, 0, Stack, frame, effects,
                result) || result != 0xFEDCBA9876543210ull ||
            frame.lr != 0x11223344u || effects.f0 != 1.25 ||
            !services.events.empty())
            return 1;
        std::printf("PASS instance-connection %u original-PPC cases; unknown entry\n",
            cases);
        std::puts("LIMIT mapped property/archive/resize/fill dependencies use prior semantic models; ordinary RAM and finite FP; no runtime scene");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
