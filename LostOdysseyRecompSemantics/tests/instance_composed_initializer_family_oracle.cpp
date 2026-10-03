#include "lo_semantics/instance_composed_initializer_family.h"
#include "lo_semantics/registered_metadata_words.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace composed = lo::semantic::gpu::instance_composed_initializer_family;
namespace metadata = lo::semantic::gpu::registered_metadata_words;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x30000u;
constexpr GuestAddress AlternateObject = 0x48000u;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Allocation = 0x40000u;
constexpr GuestAddress Manager = 0x15000u;
constexpr GuestAddress ManagerVtable = 0x15100u;
constexpr GuestAddress Data = 0x20000u;
constexpr GuestAddress Replacement = 0x21000u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress Gate = 0x83313660u;
constexpr GuestAddress Array = 0x8336b110u;
constexpr GuestAddress F13Source = 0x8218958cu;
constexpr GuestAddress F0Source = 0x82000e50u;
constexpr GuestAddress AllocateMethod = 0x82345678u;
constexpr GuestAddress ResizeMethod = 0x82345680u;
constexpr std::array<GuestAddress, 4> HighPages = {
    0x8330b000u, 0x83313000u, 0x8336b000u, 0x82189000u,
};

enum class Mode { Basic, Null, FlagSet, AllocateFail, SpillAlias,
                  FpAlias, GateReady, AppendGrowMutate, GateAlias };

struct Case
{
    GuestAddress address;
    PPCFunc* original;
    Mode mode;
    GuestAddress object = Object;
};

constexpr Case Cases[] = {
/* CASE_TABLE */
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x80000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x82000000u, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve composed guest window");
        for (GuestAddress page : HighPages)
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit composed guest page");
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
    std::array<std::uint64_t, 4> arguments;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices, ArrayResizeServices,
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
    { throw std::runtime_error("unexpected manager constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback manager constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager init method"); }
    void InitializeManager() override
    { throw std::runtime_error("unexpected array manager initialization"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
                                std::uint64_t) override
    { throw std::runtime_error("unexpected release"); }
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t bytes, std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        if (method != AllocateMethod || manager != Manager || bytes != 12 ||
            alignment != 8)
            throw std::runtime_error("wrong manager allocation arguments");
        if (test.mode == Mode::SpillAlias)
        {
            memory.WriteU32(Stack - 112u, 0);
            memory.WriteU32(Stack - 108u, AlternateObject);
        }
        return test.mode == Mode::AllocateFail ? 0x1234567800000000ull :
            0x1234567800040000ull;
    }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress storage, std::uint32_t bytes, std::uint32_t argument) override
    {
        events.push_back({'R', {method, manager, storage,
                               (std::uint64_t{bytes} << 32) | argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8)
            throw std::runtime_error("wrong metadata resize arguments");
        if (test.mode == Mode::AppendGrowMutate)
        {
            const GuestAddress outgoing = test.address == 0x8272ce60u ?
                Stack - 128u : Stack - 32u;
            memory.WriteU32(outgoing, 0x76543210u);
            if (test.address == 0x8272ce60u)
            {
                memory.WriteU32(Stack - 112u, 0);
                memory.WriteU32(Stack - 108u, AlternateObject);
            }
        }
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
    std::memset(window.bytes, 0xbd, 0x80000);
    std::memset(window.bytes + 0x82000000u, 0xbd, 0x1000);
    for (GuestAddress page : HighPages)
        std::memset(window.bytes + page, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(ManagerVtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Gate, test.mode == Mode::GateReady ? 1u : 0u);
    memory.WriteU32(Array, Data);
    memory.WriteU32(Array + 4u, test.mode == Mode::AppendGrowMutate ? 2u : 0u);
    memory.WriteU32(Array + 8u, test.mode == Mode::AppendGrowMutate ? 2u : 3u);
    memory.WriteU32(F13Source, 0xc0100000u);
    memory.WriteU32(F0Source, 0x3fc00000u);
    memory.WriteU32(test.object + 8u, 0);
    memory.WriteU32(test.object + 12u, test.mode == Mode::FlagSet ? 0x200u : 0u);
}

bool EqualPages(const Window& original, const Window& recovered)
{
    if (std::memcmp(original.bytes, recovered.bytes, 0x80000)) return false;
    if (std::memcmp(original.bytes + 0x82000000u,
                    recovered.bytes + 0x82000000u, 0x1000)) return false;
    for (GuestAddress page : HighPages)
        if (std::memcmp(original.bytes + page, recovered.bytes + page, 0x1000))
            return false;
    return true;
}

bool Compare(const Case& test)
{
    HostFpGuard host;
    Window original, recovered;
    Initialize(original, test);
    Initialize(recovered, test);
    PPCContext raw{};
    raw.r3.u64 = test.mode == Mode::Null ? 0x1234567800000000ull :
        0x1234567800000000ull | test.object;
    raw.r1.u64 = Stack;
    raw.lr = 0x1234567887654321ull;
    raw.r30.u64 = 0xaabbccdd11223344ull;
    raw.r31.u64 = 0x5566778899aabbccull;
    raw.fpscr.csr = host.saved | PPCFPSCRRegister::FlushMask;
    PPCContext restored = raw;
    raw.fpscr.setcsr(raw.fpscr.csr);
    Services expected(original.bytes, raw, test);
    active = &expected;
    test.original(raw, original.bytes);
    host.control.setcsr(host.saved);

    restored.fpscr.setcsr(restored.fpscr.csr);
    Services actual(recovered.bytes, restored, test);
    composed::FrameRegisters frame{restored.lr, restored.r30.u64,
                                   restored.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!composed::Apply(test.address, actual.memory, actual, actual, actual,
            restored.r3.u64, Stack, frame, result))
        throw std::runtime_error("composed entry not mapped");
    restored.r3.u64 = result;
    restored.lr = frame.lr;
    restored.r30.u64 = frame.r30;
    restored.r31.u64 = frame.r31;
    const bool same = raw.r3.u64 == restored.r3.u64 &&
        raw.lr == restored.lr && raw.r30.u64 == restored.r30.u64 &&
        raw.r31.u64 == restored.r31.u64 &&
        (raw.fpscr.csr & PPCFPSCRRegister::FlushMask) ==
        (restored.fpscr.csr & PPCFPSCRRegister::FlushMask) &&
        expected.events == actual.events && EqualPages(original, recovered);
    if (!same)
        std::fprintf(stderr, "FAIL composed %08x mode=%d raw-r3=%016llx recovered=%016llx\n",
            test.address, static_cast<int>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(restored.r3.u64));
    return same;
}
} // namespace

PPC_FUNC(sub_82486C88)
{
    ctx.r3.u64 = AllocateManagerBuffer(active->memory, *active,
        ctx.r3.u64, ctx.r1.u32);
}
PPC_FUNC(sub_825F41E8)
{
    ctx.r3.u64 = metadata::AppendMetadataWord(active->memory, *active,
        ctx.r3.u32, ctx.r4.u32);
}
PPC_FUNC(sub_825A56D0) { __imp__sub_825A56D0(ctx, base); }
PPC_FUNC(sub_826DB6A8) { __imp__sub_826DB6A8(ctx, base); }
PPC_FUNC(sub_826B50C8) { __imp__sub_826B50C8(ctx, base); }
PPC_FUNC(sub_826D6C30) { __imp__sub_826D6C30(ctx, base); }

int main()
{
    try
    {
        unsigned count = 0;
        for (const Case& test : Cases)
        {
            if (!Compare(test)) return 1;
            ++count;
        }
        std::array<std::uint8_t, 32> bytes{};
        GuestMemory memory(0, bytes);
        PPCContext context{};
        Case test{0xffffffffu, nullptr, Mode::Basic};
        Window spare;
        Services services(spare.bytes, context, test);
        composed::FrameRegisters frame{1, 2, 3};
        std::uint64_t result = 0xdeadbeefcafef00dull;
        if (composed::Apply(0xffffffffu, memory, services, services, services,
                Object, Stack, frame, result) ||
            result != 0xdeadbeefcafef00dull || frame.lr != 1 ||
            frame.r30 != 2 || frame.r31 != 3 ||
            bytes != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown composed address changed state");
        ++count;
        std::printf("PASS instance-composed 11 exact bodies, %u bounded comparisons\n", count);
        std::puts("LIMIT lower helper models reused; generic helper ABI and signaling-NaN baseline excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
