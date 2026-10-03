#include "lo_semantics/instance_owned_state_family.h"

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
namespace owned = lo::semantic::gpu::owned_state_initializer;
namespace family = lo::semantic::gpu::instance_owned_state_family;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Alternate = 0x12000u;
constexpr GuestAddress NewState = 0x30000u;
constexpr GuestAddress OldState = 0x40000u;
constexpr GuestAddress OldVtable = 0x40100u;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress ManagerVtable = 0x50100u;
constexpr GuestAddress Stack = 0x70000u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress FloatSource = 0x82000e50u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress DestroyMethod = 0x82345690u;

enum class Mode { Ordinary, Null, SkipState, CallbackMutation,
                  FrameAlias, AllocationFailure, HighSpCallback };
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
        if (!bytes || !VirtualAlloc(bytes, 0x80000, MEM_COMMIT,
                                    PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x82000000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit owned-state caller guest window");
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

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

struct Services final : ManagerFacadeServices, owned::StateServices,
                        instance_component_initializer_family::ComponentFpServices
{
    GuestMemory memory;
    PPCContext& context;
    const Case& test;
    std::vector<Event> events;
    unsigned fp_calls{};
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
        if (method != AllocateMethod || manager != Manager || bytes != 56 ||
            alignment != 8)
            throw std::runtime_error("wrong state allocation callback");
        return test.mode == Mode::AllocationFailure ?
            0x1234567800000000ull : 0x1234567800030000ull;
    }
    void DestroyState(GuestAddress method, GuestMemory& callback_memory,
        std::uint64_t receiver, std::uint64_t argument,
        std::uint64_t caller_sp, owned::FrameRegisters& frame) override
    {
        if (&callback_memory != &memory)
            throw std::runtime_error("wrong destroy callback memory");
        events.push_back({'D', {method, receiver, argument, caller_sp,
                                frame.lr, frame.r31}});
        if (method != DestroyMethod || receiver != OldState || argument != 1 ||
            frame.lr != 0x823fa03cu)
            throw std::runtime_error("wrong dynamic destructor boundary");
        if (test.mode == Mode::CallbackMutation)
        {
            frame.r31 = 0xabcdef0000012000ull;
            WriteU64(memory, static_cast<GuestAddress>(caller_sp) + 80u,
                     frame.r31);
        }
        if (test.mode == Mode::FrameAlias)
            WriteU64(memory, Stack - 16u, 0xaabbccdd00012000ull);
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
    std::memset(window.bytes, 0xbd, 0x80000);
    std::memset(window.bytes + 0x82000000u, 0xbd, 0x1000);
    std::memset(window.bytes + 0x8330b000u, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(OldState, OldVtable);
    memory.WriteU32(OldVtable, DestroyMethod | 3u);
    memory.WriteU32(FloatSource, 0x3fc00000u);
    for (GuestAddress object : {Object, Alternate})
    {
        WriteU64(memory, object + 8u,
            test.mode == Mode::SkipState ? 0x1122334400000200ull :
                0x1122334400000000ull);
        memory.WriteU32(object + 24u,
            test.mode == Mode::CallbackMutation ||
            test.mode == Mode::FrameAlias ||
            test.mode == Mode::HighSpCallback ? OldState : 0u);
        memory.WriteU32(object + 52u, 0x55667788u);
        for (GuestAddress offset : {184u, 200u, 216u, 232u})
            memory.WriteU32(object + offset, 0x00112233u + offset);
    }
}

bool SameMemory(const Window& first, const Window& second)
{
    return std::memcmp(first.bytes, second.bytes, 0x80000) == 0 &&
        std::memcmp(first.bytes + 0x82000000u,
                    second.bytes + 0x82000000u, 0x1000) == 0 &&
        std::memcmp(first.bytes + 0x8330b000u,
                    second.bytes + 0x8330b000u, 0x1000) == 0;
}

bool Compare(const Case& test)
{
    HostFpGuard guard;
    Window original, recovered;
    Initialize(original, test);
    Initialize(recovered, test);
    PPCContext raw{};
    raw.r1.u64 = test.mode == Mode::HighSpCallback ?
        0x1234567800070000ull : Stack;
    raw.r3.u64 = 0xabcdef0000000000ull |
        (test.mode == Mode::Null ? 0u : Object);
    raw.r4.u64 = 0x1234567800000022ull;
    raw.r31.u64 = 0x9988776655443322ull;
    raw.lr = 0x1122334455667788ull;
    raw.f0.f64 = 42.25;
    raw.fpscr.csr = guard.saved | PPCFPSCRRegister::FlushMask;
    PPCContext state = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    Services expected(original.bytes, raw, test);
    active = &expected;
    test.original(raw, original.bytes);
    guard.control.setcsr(guard.saved);

    state.fpscr.setcsr(state.fpscr.csr);
    Services actual(recovered.bytes, state, test);
    family::FrameRegisters frame{state.lr, state.r31.u64};
    std::uint64_t f0_bits = std::bit_cast<std::uint64_t>(state.f0.f64);
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(test.address, actual.memory, actual, actual, actual,
                       state.r3.u64, state.r4.u64, state.r1.u64, frame,
                       f0_bits, result))
        throw std::runtime_error("owned-state caller mapping missing");
    state.r3.u64 = result;
    state.lr = frame.lr;
    state.r31.u64 = frame.r31;
    state.f0.f64 = std::bit_cast<double>(f0_bits);
    const bool same = raw.r1.u64 == state.r1.u64 &&
        raw.r3.u64 == state.r3.u64 && raw.lr == state.lr &&
        raw.r31.u64 == state.r31.u64 &&
        std::bit_cast<std::uint64_t>(raw.f0.f64) == f0_bits &&
        (raw.fpscr.csr & PPCFPSCRRegister::FlushMask) ==
        (state.fpscr.csr & PPCFPSCRRegister::FlushMask) &&
        expected.events == actual.events && SameMemory(original, recovered);
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL owned-state caller %08x mode %d r3 %016llx/%016llx "
            "r31 %016llx/%016llx f0 %016llx/%016llx fpscr %08x/%08x "
            "events %zu/%zu\n", test.address,
            static_cast<int>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(state.r3.u64),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(state.r31.u64),
            static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(raw.f0.f64)),
            static_cast<unsigned long long>(f0_bits), raw.fpscr.csr,
            state.fpscr.csr,
            expected.events.size(), actual.events.size());
        for (GuestAddress address = 0; address < 0x80000; ++address)
            if (original.bytes[address] != recovered.bytes[address])
            {
                std::fprintf(stderr, "first memory diff %08x %02x/%02x\n",
                    address, original.bytes[address], recovered.bytes[address]);
                break;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(sub_82486C88)
{
    ctx.r3.u64 = AllocateManagerBuffer(active->memory, *active,
        ctx.r3.u64, ctx.r1.u32);
}
PPC_FUNC(sub_82406A38) { __imp__sub_82406A38(ctx, base); }
PPC_FUNC(sub_823FA008) { __imp__sub_823FA008(ctx, base); }
PPC_FUNC(sub_827134B8) { __imp__sub_827134B8(ctx, base); }
PPC_FUNC(sub_82693E38) { __imp__sub_82693E38(ctx, base); }

void OwnedStateIndirect(PPCContext& ctx, std::uint8_t*, std::uint32_t method)
{
    owned::FrameRegisters frame{ctx.lr, ctx.r31.u64};
    active->DestroyState(method, active->memory, ctx.r3.u64, ctx.r4.u64,
                         ctx.r1.u64, frame);
    ctx.lr = frame.lr;
    ctx.r31.u64 = frame.r31;
    ctx.r3.u64 = 0x123456789abcdef0ull;
}

int main()
{
    try
    {
        for (const Case& test : Cases)
            if (!Compare(test))
                return 1;
        std::array<std::uint8_t, 32> untouched{};
        GuestMemory memory(0, untouched);
        Window spare;
        Case unknown{0xffffffffu, nullptr, Mode::Ordinary};
        PPCContext context{};
        Services services(spare.bytes, context, unknown);
        family::FrameRegisters frame{1, 2};
        std::uint64_t f0_bits = 3;
        std::uint64_t result = 4;
        if (family::Apply(0xffffffffu, memory, services, services,
                          services, Object, 0, Stack, frame,
                          f0_bits, result) || frame.lr != 1 ||
            frame.r31 != 2 || f0_bits != 3 || result != 4 ||
            untouched != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown owned-state caller changed state");
        std::printf("PASS instance-owned-state 8 exact bodies, %zu bounded comparisons\n",
                    std::size(Cases) + 1);
        std::puts("LIMIT dynamic destructor callback, prior allocation model, "
                  "generic helper ABI/volatile GPR/CR, optimized sNaN baseline");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
