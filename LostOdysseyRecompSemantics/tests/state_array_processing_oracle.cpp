#include "lo_semantics/state_array_processing.h"

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
namespace state = lo::semantic::gpu::state_array_processing;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Node1 = 0x20000u;
constexpr GuestAddress Node2 = 0x20200u;
constexpr GuestAddress ItemVtable = 0x28000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerVtable = 0x30100u;
constexpr GuestAddress StorageA = 0x50000u;
constexpr GuestAddress StorageB = 0x58000u;
constexpr GuestAddress StorageC = 0x60000u;
constexpr GuestAddress Stack = 0x70000u;
constexpr GuestAddress Counter = 0x83315ee4u;
constexpr GuestAddress Head = 0x83315ef0u;
constexpr GuestAddress ParentFlag = 0x83315edcu;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress ItemMethod = 0x82345670u;
constexpr GuestAddress ResizeMethod = 0x82345680u;
constexpr GuestAddress ReleaseMethod = 0x82345690u;
constexpr GuestAddress InitFirst = 0x823456a0u;
constexpr GuestAddress InitSecond = 0x823456b0u;

enum class Mode { Empty, One, NestedQueue, DirectGrowth, CleanupResidual,
                  CleanupNestedResidual,
                  Release, SavedAlias, LazyManager, ParentActive,
                  ParentFlagSet, ParentField72, ParentField40,
                  ParentFrameAlias, TailNull, TailActive };
struct Case { GuestAddress address; Mode mode; };
constexpr Case Cases[] = {
    {0x823fdab0u, Mode::Empty},
    {0x823fdab0u, Mode::One},
    {0x823fdab0u, Mode::NestedQueue},
    {0x823fdab0u, Mode::DirectGrowth},
    {0x823fdab0u, Mode::CleanupResidual},
    {0x823fdab0u, Mode::CleanupNestedResidual},
    {0x823fdab0u, Mode::Release},
    {0x823fdab0u, Mode::SavedAlias},
    {0x823fdab0u, Mode::LazyManager},
    {0x82407ad8u, Mode::ParentActive},
    {0x82407ad8u, Mode::ParentFlagSet},
    {0x82407ad8u, Mode::ParentField72},
    {0x82407ad8u, Mode::ParentField40},
    {0x82407ad8u, Mode::ParentFrameAlias},
    {0x8240a7c0u, Mode::TailNull},
    {0x8240a7c0u, Mode::TailActive},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000, MEM_COMMIT,
                                    PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83315000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit state-array guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

struct Event
{
    char kind;
    std::array<std::uint64_t, 7> registers;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerInitServices, state::StateArrayCallbacks
{
    GuestMemory memory;
    const Case& test;
    std::uint64_t hub_sp;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, const Case& selected,
             std::uint64_t current_hub_sp)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), test(selected),
          hub_sp(current_hub_sp) {}

    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        events.push_back({'A', {bytes, 0, 0, 0, 0, 0, 0}});
        if (test.mode != Mode::LazyManager || bytes != 0x48decu)
            throw std::runtime_error("unexpected manager raw allocation");
        return Manager;
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    {
        events.push_back({'C', {allocation, 0, 0, 0, 0, 0, 0}});
        if (allocation != Manager)
            throw std::runtime_error("unexpected manager construction");
        return allocation;
    }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected manager fallback"); }
    std::uint64_t CallMethod(GuestAddress method,
                             std::uint64_t receiver) override
    {
        events.push_back({'M', {method, receiver, 0, 0, 0, 0, 0}});
        if (method != InitFirst && method != InitSecond)
            throw std::runtime_error("unexpected manager init method");
        return 1;
    }

    void VisitItem(GuestAddress method, GuestMemory& supplied,
        std::uint64_t receiver, std::uint64_t caller_sp,
        state::FrameRegisters& frame) override
    {
        if (&supplied != &memory || method != ItemMethod ||
            receiver != Node1 || caller_sp != hub_sp ||
            frame.lr != 0x823fdba0u)
            throw std::runtime_error("wrong item callback contract");
        events.push_back({'I', {method, receiver, 0, 0, 0,
                                caller_sp, frame.lr}});
        if (test.mode == Mode::NestedQueue)
            memory.WriteU32(Head, Node2);
        if (test.mode == Mode::DirectGrowth)
        {
            for (unsigned i = 0; i < 34; ++i)
            {
                const GuestAddress node = Node2 + i * 0x40u;
                memory.WriteU32(node + 32u, i == 33 ? 0 : node + 0x40u);
            }
            memory.WriteU32(Head, Node2);
        }
        if (test.mode == Mode::SavedAlias)
        {
            memory.WriteU32(static_cast<GuestAddress>(caller_sp + 184u),
                            0x13572468u);
            WriteU64(memory, static_cast<GuestAddress>(caller_sp + 96u),
                     0xabcdef0000000001ull);
        }
    }

    std::uint64_t ResizeStorage(GuestAddress method, GuestMemory& supplied,
        std::uint64_t manager, std::uint64_t storage, std::uint64_t bytes,
        std::uint64_t argument, std::uint64_t caller_sp,
        state::FrameRegisters& frame) override
    {
        if (&supplied != &memory || method != ResizeMethod ||
            manager != Manager || argument != 8)
            throw std::runtime_error("wrong state-array resize contract");
        events.push_back({'R', {method, manager, storage, bytes, argument,
                                caller_sp, frame.lr}});
        if (caller_sp == hub_sp)
        {
            if (frame.lr == 0x823fdc18u)
            {
                if (test.mode != Mode::DirectGrowth || bytes == 0 ||
                    storage != StorageA)
                    throw std::runtime_error("unexpected direct growth");
                for (unsigned i = 0; i < 33; ++i)
                    memory.WriteU32(StorageB + i * 4u,
                                    memory.ReadU32(StorageA + i * 4u));
                return 0xabcdef0000058000ull;
            }
            if (frame.lr != 0x823fdca8u || bytes != 0)
                throw std::runtime_error("unexpected direct cleanup");
            if (test.mode == Mode::CleanupResidual ||
                test.mode == Mode::CleanupNestedResidual)
            {
                const GuestAddress header = static_cast<GuestAddress>(hub_sp + 80u);
                memory.WriteU32(header + 4u,
                    test.mode == Mode::CleanupResidual ? 0u : 2u);
                memory.WriteU32(header + 8u,
                    test.mode == Mode::CleanupResidual ? 65u : 128u);
                return 0;
            }
            if (test.mode == Mode::Release)
                return 0x1234567800060000ull;
            return 0;
        }
        if (caller_sp != hub_sp - 128u && caller_sp != hub_sp - 256u)
            throw std::runtime_error("wrong nested resize SP");
        if (frame.lr != 0x8229f6e0u)
            throw std::runtime_error("wrong nested resize LR");
        if (test.mode == Mode::CleanupNestedResidual &&
            caller_sp == hub_sp - 256u)
        {
            if (bytes != 8 || storage != 0)
                throw std::runtime_error("wrong nested remove-resize registers");
            return 0x1234567800000000ull;
        }
        return StorageA;
    }

    std::uint64_t ReleaseStorage(GuestAddress method, GuestMemory& supplied,
        std::uint64_t manager, std::uint64_t storage,
        std::uint64_t caller_sp, state::FrameRegisters& frame) override
    {
        if (&supplied != &memory || method != ReleaseMethod ||
            manager != Manager || caller_sp != hub_sp ||
            frame.lr != 0x823fdd04u)
            throw std::runtime_error("wrong state-array release contract");
        events.push_back({'F', {method, manager, storage, 0, 0,
                                caller_sp, frame.lr}});
        return 0x87654321abcdef90ull;
    }
};

Services* active = nullptr;

state::FrameRegisters FromContext(const PPCContext& ctx)
{
    return {ctx.lr, {ctx.r21.u64, ctx.r22.u64, ctx.r23.u64,
        ctx.r24.u64, ctx.r25.u64, ctx.r26.u64, ctx.r27.u64,
        ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64}};
}

void ToContext(PPCContext& ctx, const state::FrameRegisters& frame)
{
    ctx.lr = frame.lr;
    ctx.r21.u64 = frame.gpr[0]; ctx.r22.u64 = frame.gpr[1];
    ctx.r23.u64 = frame.gpr[2]; ctx.r24.u64 = frame.gpr[3];
    ctx.r25.u64 = frame.gpr[4]; ctx.r26.u64 = frame.gpr[5];
    ctx.r27.u64 = frame.gpr[6]; ctx.r28.u64 = frame.gpr[7];
    ctx.r29.u64 = frame.gpr[8]; ctx.r30.u64 = frame.gpr[9];
    ctx.r31.u64 = frame.gpr[10];
}

void Initialize(Window& window, const Case& test, GuestAddress object)
{
    std::memset(window.bytes, 0xbd, 0x90000);
    std::memset(window.bytes + 0x8330b000u, 0xbd, 0x1000);
    std::memset(window.bytes + 0x83315000u, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal,
        test.mode == Mode::LazyManager ? 0 : Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(ManagerVtable + 12u, ReleaseMethod | 3u);
    memory.WriteU32(ManagerVtable + 56u, InitSecond | 3u);
    memory.WriteU32(ManagerVtable + 60u, InitFirst | 3u);
    memory.WriteU32(ItemVtable + 124u, ItemMethod | 3u);
    memory.WriteU32(Counter, 4);
    memory.WriteU32(ParentFlag, 0);
    const bool queue = test.mode == Mode::One ||
        test.mode == Mode::NestedQueue || test.mode == Mode::DirectGrowth ||
        test.mode == Mode::CleanupResidual ||
        test.mode == Mode::CleanupNestedResidual ||
        test.mode == Mode::Release ||
        test.mode == Mode::SavedAlias || test.mode == Mode::LazyManager;
    memory.WriteU32(Head, queue ? Node1 : 0);
    memory.WriteU32(Node1, ItemVtable);
    memory.WriteU32(Node1 + 4u,
        test.mode == Mode::NestedQueue || test.mode == Mode::DirectGrowth ||
        test.mode == Mode::SavedAlias ? 0xffffffffu : 0u);
    memory.WriteU32(Node1 + 32u, 0);
    memory.WriteU32(Node2 + 4u, 0);
    memory.WriteU32(Node2 + 32u, 0);
    memory.WriteU32(object + 12u,
        test.mode == Mode::ParentFlagSet ? 0x200u : 0u);
    memory.WriteU32(object + 72u,
        test.mode == Mode::ParentField72 ? 1u : 0u);
    memory.WriteU32(object + 40u,
        test.mode == Mode::ParentField40 ? 1u : 0u);
}

bool Compare(const Case& test, Window& original, Window& recovered)
{
    const bool parent = test.address != 0x823fdab0u;
    const GuestAddress object = test.mode == Mode::ParentFrameAlias ?
        Stack - 24u : Object;
    Initialize(original, test, object);
    Initialize(recovered, test, object);
    const std::uint64_t caller_sp = test.mode == Mode::CleanupResidual ?
        0x1234567800070000ull : Stack;
    const std::uint64_t hub_sp = caller_sp - (parent ? 304u : 192u);
    Services expected(original.bytes, test, hub_sp);
    Services actual(recovered.bytes, test, hub_sp);
    const std::uint64_t incoming_r3 = test.mode == Mode::TailNull ?
        0x1234567800000000ull :
        0xabcdef0000000000ull | object;
    PPCContext context{};
    context.r1.u64 = caller_sp;
    context.r3.u64 = incoming_r3;
    context.lr = 0x8765432182345678ull;
    for (unsigned i = 0; i < 11; ++i)
        (&context.r21)[i].u64 = 0x1111000000000000ull | (i * 0x1000u + 0x55u);
    const state::FrameRegisters initial = FromContext(context);
    active = &expected;
    switch (test.address)
    {
    case 0x823fdab0u: __imp__sub_823FDAB0(context, original.bytes); break;
    case 0x82407ad8u: __imp__sub_82407AD8(context, original.bytes); break;
    case 0x8240a7c0u: __imp__sub_8240A7C0(context, original.bytes); break;
    default: throw std::runtime_error("wrong state-array case address");
    }
    state::FrameRegisters frame = initial;
    std::uint64_t result = 0xdeadbeefcafebabeull;
    active = &actual;
    if (!state::Apply(test.address, actual.memory, actual, actual,
                      incoming_r3, caller_sp, frame, result))
        throw std::runtime_error("missing state-array entry");
    const auto observed = FromContext(context);
    bool same = context.r3.u64 == result &&
        observed.lr == frame.lr && observed.gpr == frame.gpr &&
        context.r1.u64 == caller_sp && expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x90000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
                    recovered.bytes + 0x8330b000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83315000u,
                    recovered.bytes + 0x83315000u, 0x1000) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL state-array %08x mode %u r3 %llx/%llx LR %llx/%llx events %zu/%zu\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size());
        for (std::size_t i = 0, shown = 0; i < 0x90000 && shown < 8; ++i)
            if (original.bytes[i] != recovered.bytes[i])
            {
                std::fprintf(stderr, "  memory %08zx %02x/%02x\n", i,
                    original.bytes[i], recovered.bytes[i]);
                ++shown;
            }
        for (std::size_t i = 0; i < expected.events.size(); ++i)
            if (i >= actual.events.size() ||
                !(expected.events[i] == actual.events[i]))
            {
                std::fprintf(stderr, "  first event mismatch %zu kind %c/%c\n",
                    i, expected.events[i].kind,
                    i < actual.events.size() ? actual.events[i].kind : '-');
                break;
            }
    }
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_21) { __imp____savegprlr_21(ctx, base); }
PPC_FUNC(__restgprlr_21) { __imp____restgprlr_21(ctx, base); }
PPC_FUNC(__savegprlr_27) { __imp____savegprlr_27(ctx, base); }
PPC_FUNC(__restgprlr_27) { __imp____restgprlr_27(ctx, base); }
PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
PPC_FUNC(sub_823FDAB0) { __imp__sub_823FDAB0(ctx, base); }
PPC_FUNC(sub_82407AD8) { __imp__sub_82407AD8(ctx, base); }
PPC_FUNC(sub_8229F678) { __imp__sub_8229F678(ctx, base); }
PPC_FUNC(sub_82298AF8) { __imp__sub_82298AF8(ctx, base); }
PPC_FUNC(sub_82B7C470) { __imp__sub_82B7C470(ctx, base); }
PPC_FUNC(sub_82B7A0B0)
{ throw std::runtime_error("zero-index remove unexpectedly copied bytes"); }
PPC_FUNC(sub_827C5F38)
{
    const std::uint64_t caller_sp = ctx.r1.u64;
    active->memory.WriteU32(ctx.r1.u32 - 8u,
                            static_cast<std::uint32_t>(ctx.lr));
    WriteU64(active->memory, ctx.r1.u32 - 24u, ctx.r30.u64);
    WriteU64(active->memory, ctx.r1.u32 - 16u, ctx.r31.u64);
    active->memory.WriteU32(ctx.r1.u32 - 112u, ctx.r1.u32);
    ctx.r1.u64 -= 112u;
    ctx.r3.u64 = InitializeManager(active->memory, *active, ctx.r1.u32);
    ctx.r1.u64 = caller_sp;
    ctx.lr = active->memory.ReadU32(ctx.r1.u32 - 8u);
    ctx.r30.u64 = (std::uint64_t{active->memory.ReadU32(ctx.r1.u32 - 24u)} << 32) |
                  active->memory.ReadU32(ctx.r1.u32 - 20u);
    ctx.r31.u64 = (std::uint64_t{active->memory.ReadU32(ctx.r1.u32 - 16u)} << 32) |
                  active->memory.ReadU32(ctx.r1.u32 - 12u);
}

void StateArrayIndirect(PPCContext& ctx, std::uint8_t*, std::uint32_t method)
{
    state::FrameRegisters frame = FromContext(ctx);
    if (method == ItemMethod)
    {
        active->VisitItem(method, active->memory, ctx.r3.u64, ctx.r1.u64, frame);
        ctx.r3.u64 = 0x123456789abcdef0ull; // ignored by the caller.
    }
    else if (method == ResizeMethod)
        ctx.r3.u64 = active->ResizeStorage(method, active->memory, ctx.r3.u64,
            ctx.r4.u64, ctx.r5.u64, ctx.r6.u64, ctx.r1.u64, frame);
    else if (method == ReleaseMethod)
        ctx.r3.u64 = active->ReleaseStorage(method, active->memory, ctx.r3.u64,
            ctx.r4.u64, ctx.r1.u64, frame);
    else
        throw std::runtime_error("unexpected state-array indirect target");
    ToContext(ctx, frame);
}

int main()
{
    try
    {
        Window original, recovered;
        for (const Case& test : Cases)
            if (!Compare(test, original, recovered)) return 1;
        std::array<std::uint8_t, 32> bytes{};
        GuestMemory memory(0, bytes);
        const Case unknown{0xffffffffu, Mode::Empty};
        Services untouched(recovered.bytes, unknown, Stack - 192u);
        state::FrameRegisters frame{7, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}};
        std::uint64_t result = 0x1234u;
        if (state::Apply(unknown.address, memory, untouched, untouched,
                         Object, Stack, frame, result) ||
            result != 0x1234u || frame.lr != 7 ||
            frame.gpr != std::array<std::uint64_t, 11>{1, 2, 3, 4, 5, 6, 7,
                                                        8, 9, 10, 11} ||
            bytes != std::array<std::uint8_t, 32>{} ||
            !untouched.events.empty())
            throw std::runtime_error("unknown state-array entry changed state");
        std::printf("PASS state-array %zu original-PPC comparisons and 1 unknown\n",
                    std::size(Cases));
        std::puts("LIMIT three exact PPC bodies plus known ABI helpers and lower algorithms; dynamic callbacks, untracked item volatile r4-r7, generic lower ABI, faults/MMIO/concurrency excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
