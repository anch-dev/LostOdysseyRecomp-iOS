#include "lo_semantics/instance_navigation_allocation_family.h"

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
namespace family = lo::semantic::gpu::instance_navigation_allocation_family;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Owner = 0x10000u;
constexpr GuestAddress Node = 0x30000u;
constexpr GuestAddress Alternate = 0x40000u;
constexpr GuestAddress LinkVtable = 0x15000u;
constexpr GuestAddress AlternateVtable = 0x15100u;
constexpr GuestAddress Receiver = 0x50000u;
constexpr GuestAddress ReceiverVtable = 0x50100u;
constexpr GuestAddress Manager = 0x51000u;
constexpr GuestAddress ManagerVtable = 0x51100u;
constexpr GuestAddress Source = 0x52000u;
constexpr GuestAddress Stack = 0x70000u;
constexpr GuestAddress FirstMethod = 0x82345680u;
constexpr GuestAddress SecondMethod = 0x82345688u;
constexpr GuestAddress OwnerMethod = 0x82345690u;
constexpr GuestAddress AllocateMethod = 0x823456a0u;

enum class Mode { LinkPlain, LinkAlias, LinkCallbacks, NodeInitializer,
                  Allocation, OwnerReady, OwnerSuccess, TailNull,
                  TailFailure, AllocationMutateR30 };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
/* CASE_TABLE */
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes)
            throw std::runtime_error("reserve navigation guest window");
        for (std::uint64_t address : {0ull, 0x82000000ull, 0x82189000ull,
             0x821da000ull, 0x8330b000ull, 0x83315000ull,
             0x83318000ull, 0x8336c000ull})
        {
            const std::size_t size = address == 0 ? 0x80000 : 0x1000;
            if (!VirtualAlloc(bytes + address, size, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit navigation guest page");
            std::memset(bytes + address, 0xbd, size);
        }
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct HostFpGuard
{
    PPCFPSCRRegister control{};
    std::uint32_t saved = control.getcsr();
    ~HostFpGuard() { control.setcsr(saved); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 6> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices, family::VirtualServices,
                        instance_component_initializer_family::ComponentFpServices
{
    GuestMemory memory;
    PPCContext& context;
    const Case& test;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, PPCContext& cpu, const Case& selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)),
          context(cpu), test(selected) {}

    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw manager allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback manager construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager init method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
                                 std::uint64_t) override
    { throw std::runtime_error("unexpected storage release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
                               std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment, 0, 0}});
        if (method != AllocateMethod || manager != Manager || bytes != 448 ||
            alignment != 8)
            throw std::runtime_error("wrong navigation allocation boundary");
        return test.mode == Mode::TailFailure ?
            0xabcdef0000000000ull : 0xabcdef0000030000ull;
    }
    std::uint64_t Call(GuestAddress method, GuestMemory& callback_memory,
        std::uint64_t r3, std::uint64_t sp,
        family::FrameRegisters& frame) override
    {
        if (&callback_memory != &memory)
            throw std::runtime_error("wrong navigation callback memory");
        events.push_back({'V', {method, r3, sp, frame.lr, frame.r27, frame.r31}});
        if (method == FirstMethod && test.mode == Mode::LinkCallbacks)
        {
            frame.r31 = 0xabcdef0000040000ull;
            memory.WriteU32(Node, AlternateVtable);
        }
        if (method == FirstMethod && test.mode == Mode::AllocationMutateR30)
            frame.r30 = 0x1234567887654321ull;
        if (method != FirstMethod && method != SecondMethod &&
            method != OwnerMethod)
            throw std::runtime_error("unexpected navigation callback method");
        if (method == OwnerMethod)
        {
            memory.WriteU32(Owner + 136u, 0x12345678u);
            return 0x1111222200004567ull;
        }
        return method == FirstMethod ? 0x2222333300000001ull :
            0x4444555500000002ull;
    }
    void DisableFlushMode() override
    { context.fpscr.disableFlushMode(); }
};

Services* active = nullptr;

void Initialize(Window& window, const Case& test)
{
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(0x83315fb4u, Source);
    memory.WriteU32(Source + 560u, 0x12345678u);
    memory.WriteU32(0x83318090u, Receiver);
    memory.WriteU32(Receiver, ReceiverVtable);
    memory.WriteU32(ReceiverVtable, OwnerMethod | 3u);
    memory.WriteU32(0x83318070u,
        test.mode == Mode::LinkCallbacks ||
        test.mode == Mode::AllocationMutateR30 ? 1u : 0u);
    memory.WriteU32(0x8336ce04u,
        test.mode == Mode::LinkAlias ? 0x8336cdfcu : 0u);
    memory.WriteU32(Node, LinkVtable);
    memory.WriteU32(Alternate, AlternateVtable);
    memory.WriteU32(LinkVtable, FirstMethod | 3u);
    memory.WriteU32(LinkVtable + 8u, SecondMethod | 3u);
    memory.WriteU32(AlternateVtable, FirstMethod | 3u);
    memory.WriteU32(AlternateVtable + 8u, SecondMethod | 3u);
    memory.WriteU32(0x821da7a4u, FirstMethod | 3u);
    memory.WriteU32(0x821da7a4u + 8u, SecondMethod | 3u);
    memory.WriteU32(0x82000e50u, 0x3f000000u);
    memory.WriteU32(0x8218958cu, 0x3f800000u);
    memory.WriteU32(Owner + 40u, 0);
    memory.WriteU32(Owner + 120u, 0);
    // Only the ready case skips the allocation and dynamic owner call.
    memory.WriteU32(Owner + 12u,
        test.mode == Mode::OwnerReady ? 0x600u : 0u);
    memory.WriteU32(Node + 16u,
        test.mode == Mode::LinkPlain ? 0x80000000u : 0u);
}

bool SameMemory(const Window& a, const Window& b)
{
    for (std::uint64_t address : {0ull, 0x82000000ull, 0x82189000ull,
         0x821da000ull, 0x8330b000ull, 0x83315000ull,
         0x83318000ull, 0x8336c000ull})
    {
        const std::size_t size = address == 0 ? 0x80000 : 0x1000;
        if (std::memcmp(a.bytes + address, b.bytes + address, size) != 0)
            return false;
    }
    return true;
}

bool Compare(const Case& test)
{
    HostFpGuard guard;
    Window original, recovered;
    Initialize(original, test);
    Initialize(recovered, test);
    PPCContext raw{};
    raw.r1.u64 = test.mode == Mode::OwnerSuccess ?
        0x1234567800070000ull : Stack;
    raw.r3.u64 = 0xabcdef0000000000ull |
        (test.mode == Mode::TailNull ? 0u :
         test.mode == Mode::OwnerReady || test.mode == Mode::OwnerSuccess ||
         test.mode == Mode::TailFailure ? Owner : Node);
    raw.lr = 0x1122334455667788ull;
    raw.r27.u64 = 0x1234000000000027ull;
    raw.r28.u64 = 0x1234000000000028ull;
    raw.r29.u64 = 0x1234000000000029ull;
    raw.r30.u64 = 0x1234000000000030ull;
    raw.r31.u64 = 0x1234000000000031ull;
    raw.f0.f64 = 42.25;
    raw.f13.f64 = -12.5;
    raw.fpscr.csr = guard.saved | PPCFPSCRRegister::FlushMask;
    PPCContext state = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    Services expected(original.bytes, raw, test);
    active = &expected;
    test.original(raw, original.bytes);
    guard.control.setcsr(guard.saved);
    state.fpscr.setcsr(state.fpscr.csr);
    Services actual(recovered.bytes, state, test);
    family::FrameRegisters frame{state.lr, state.r27.u64, state.r28.u64,
        state.r29.u64, state.r30.u64, state.r31.u64};
    std::uint64_t f0_bits = std::bit_cast<std::uint64_t>(state.f0.f64);
    std::uint64_t f13_bits = std::bit_cast<std::uint64_t>(state.f13.f64);
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(test.address, actual.memory, actual, actual, actual,
                       state.r3.u64, state.r1.u64, frame, f0_bits,
                       f13_bits, result))
        throw std::runtime_error("navigation entry missing");
    state.r3.u64 = result;
    state.lr = frame.lr;
    state.r27.u64 = frame.r27;
    state.r28.u64 = frame.r28;
    state.r29.u64 = frame.r29;
    state.r30.u64 = frame.r30;
    state.r31.u64 = frame.r31;
    state.f0.f64 = std::bit_cast<double>(f0_bits);
    state.f13.f64 = std::bit_cast<double>(f13_bits);
    const bool same = raw.r1.u64 == state.r1.u64 &&
        raw.r3.u64 == state.r3.u64 && raw.lr == state.lr &&
        raw.r27.u64 == state.r27.u64 && raw.r28.u64 == state.r28.u64 &&
        raw.r29.u64 == state.r29.u64 && raw.r30.u64 == state.r30.u64 &&
        raw.r31.u64 == state.r31.u64 &&
        std::bit_cast<std::uint64_t>(raw.f0.f64) == f0_bits &&
        std::bit_cast<std::uint64_t>(raw.f13.f64) == f13_bits &&
        (raw.fpscr.csr & PPCFPSCRRegister::FlushMask) ==
        (state.fpscr.csr & PPCFPSCRRegister::FlushMask) &&
        expected.events == actual.events && SameMemory(original, recovered);
    if (!same)
        std::fprintf(stderr,
            "FAIL navigation %08x mode=%d r3 %016llx/%016llx "
            "r31 %016llx/%016llx events %zu/%zu\n", test.address,
            static_cast<int>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(state.r3.u64),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(state.r31.u64),
            expected.events.size(), actual.events.size());
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
PPC_FUNC(sub_823B8728) { __imp__sub_823B8728(ctx, base); }
PPC_FUNC(sub_825AEF38) { __imp__sub_825AEF38(ctx, base); }
PPC_FUNC(sub_825AF048) { __imp__sub_825AF048(ctx, base); }
PPC_FUNC(sub_825E12A0) { __imp__sub_825E12A0(ctx, base); }
PPC_FUNC(sub_82486C88)
{
    ctx.r3.u64 = AllocateManagerBuffer(active->memory, *active,
                                        ctx.r3.u64, ctx.r1.u32);
}

void NavigationIndirect(PPCContext& context, std::uint8_t*,
                        std::uint32_t method)
{
    family::FrameRegisters frame{context.lr, context.r27.u64,
        context.r28.u64, context.r29.u64, context.r30.u64, context.r31.u64};
    context.r3.u64 = active->Call(method, active->memory, context.r3.u64,
                                  context.r1.u64, frame);
    context.lr = frame.lr;
    context.r27.u64 = frame.r27;
    context.r28.u64 = frame.r28;
    context.r29.u64 = frame.r29;
    context.r30.u64 = frame.r30;
    context.r31.u64 = frame.r31;
}

int main()
{
    try
    {
        for (const Case& test : Cases)
            if (!Compare(test))
                return 1;
        Window spare;
        PPCContext context{};
        Case unknown{0xffffffffu, nullptr, Mode::LinkPlain};
        Services services(spare.bytes, context, unknown);
        family::FrameRegisters frame{1, 2, 3, 4, 5, 6};
        std::uint64_t f0 = 7, f13 = 8, result = 9;
        if (family::Apply(0xffffffffu, services.memory, services, services,
                          services, Owner, Stack, frame, f0, f13, result) ||
            frame.lr != 1 || frame.r27 != 2 || frame.r28 != 3 ||
            frame.r29 != 4 || frame.r30 != 5 || frame.r31 != 6 ||
            f0 != 7 || f13 != 8 || result != 9 || !services.events.empty())
            throw std::runtime_error("unknown navigation target changed state");
        std::printf("PASS navigation allocation 5 exact bodies, %zu bounded comparisons + unknown\n",
                    std::size(Cases));
        std::puts("LIMIT dynamic vtable targets external, ordinary RAM, prior allocation model, generic lower ABI and optimized sNaN excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
