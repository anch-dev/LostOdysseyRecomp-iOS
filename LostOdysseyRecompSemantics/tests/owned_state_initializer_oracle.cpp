#include "lo_semantics/owned_state_initializer.h"

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
namespace owned = lo::semantic::gpu::owned_state_initializer;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Owner = 0x10000u;
constexpr GuestAddress AlternateOwner = 0x12000u;
constexpr GuestAddress State = 0x20000u;
constexpr GuestAddress OldState = 0x30000u;
constexpr GuestAddress StateVtable = 0x30100u;
constexpr GuestAddress Manager = 0x35000u;
constexpr GuestAddress ManagerVtable = 0x35100u;
constexpr GuestAddress Stack = 0x70000u;
constexpr GuestAddress GlobalManager = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress DestroyMethod = 0x82345690u;
constexpr std::uint64_t FullState = 0x1234567800020000ull;
constexpr std::uint64_t InitialR31 = 0x8899aabbccddeeffull;
constexpr std::uint64_t InitialLR = 0x7654321082345678ull;

enum class Mode { Leaf, LeafNullLow, LeafOwnerAlias, HubNoOld,
                  HubFailure, HubOld, HubMutation, HubFlagsAlias,
                  HubSavedAlias };

struct Case { GuestAddress address; Mode mode; };
constexpr Case Cases[] = {
    {0x82406A38u, Mode::Leaf},
    {0x82406A38u, Mode::LeafNullLow},
    {0x82406A38u, Mode::LeafOwnerAlias},
    {0x823FA008u, Mode::HubNoOld},
    {0x823FA008u, Mode::HubFailure},
    {0x823FA008u, Mode::HubOld},
    {0x823FA008u, Mode::HubMutation},
    {0x823FA008u, Mode::HubFlagsAlias},
    {0x823FA008u, Mode::HubSavedAlias},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x80000, MEM_COMMIT,
                                    PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT,
                          PAGE_READWRITE))
            throw std::runtime_error("commit owned-state guest window");
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
    std::array<std::uint64_t, 7> arguments;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices, owned::StateServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}

    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
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
        events.push_back({'A', {method, manager, bytes, alignment,
                                memory.ReadU32(Stack - 96u), 0, 0}});
        if (method != AllocateMethod || manager != Manager || bytes != 56 ||
            alignment != 8 || memory.ReadU32(Stack - 96u) != Stack)
            throw std::runtime_error("wrong state allocation contract");
        return mode == Mode::HubFailure ? 0x1234567800000000ull : FullState;
    }

    void DestroyState(GuestAddress method, GuestMemory& callback_memory,
        std::uint64_t receiver, std::uint64_t argument,
        std::uint64_t caller_sp, owned::FrameRegisters& frame) override
    {
        if (&callback_memory != &memory)
            throw std::runtime_error("wrong destroy memory");
        events.push_back({'D', {method, receiver, argument, caller_sp,
                                frame.lr, frame.r31,
                                memory.ReadU32(static_cast<GuestAddress>(receiver))}});
        if (method != DestroyMethod || receiver != OldState || argument != 1 ||
            caller_sp != ((mode == Mode::HubMutation ?
                0x1234567800000000ull : 0ull) | (Stack - 96u)) ||
            frame.lr != 0x823fa03cu)
            throw std::runtime_error("wrong indirect state destructor contract");
        if (mode == Mode::HubMutation)
        {
            frame.r31 = 0xabcdef0000012000ull;
            frame.lr = 0x9876543212345678ull;
            memory.WriteU32(AlternateOwner + 52u, 0x11223344u);
        }
        if (mode == Mode::HubSavedAlias)
        {
            memory.WriteU32(Stack - 8u, 0x13572468u);
            WriteU64(memory, Stack - 16u, 0xaabbccdd00010000ull);
        }
    }
};

Services* active = nullptr;

void Initialize(Window& window, const Case& test)
{
    std::memset(window.bytes, 0xbdu, 0x80000);
    std::memset(window.bytes + 0x8330b000u, 0xbdu, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(GlobalManager, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(OldState, StateVtable);
    memory.WriteU32(StateVtable, DestroyMethod | 3u);
    memory.WriteU32(52u, 0x55667788u);
    memory.WriteU32(Owner + 52u, 0x91abcdefu);
    memory.WriteU32(AlternateOwner + 52u, 0x12345678u);
    WriteU64(memory, Owner + 8u, 0x0123456789abcdefull);
    WriteU64(memory, AlternateOwner + 8u, 0x0023456789abcdefull);
    const GuestAddress hub_owner = test.mode == Mode::HubFlagsAlias ?
        Stack - 24u : Owner;
    memory.WriteU32(hub_owner + 24u,
        test.mode == Mode::HubNoOld || test.mode == Mode::HubFlagsAlias ?
        0u : OldState);
    if (test.mode == Mode::HubFlagsAlias)
        memory.WriteU32(hub_owner + 52u, 0x24681357u);
}

bool Compare(const Case& test, Window& original, Window& recovered)
{
    Initialize(original, test);
    Initialize(recovered, test);
    Services expected(original.bytes, test.mode);
    Services actual(recovered.bytes, test.mode);
    const bool leaf = test.address == 0x82406a38u;
    const GuestAddress object = leaf ? State :
        test.mode == Mode::HubFlagsAlias ? Stack - 24u : Owner;
    const std::uint64_t incoming_r3 =
        (leaf ? 0x1234567800000000ull : 0xabcdef0000000000ull) | object;
    const std::uint64_t incoming_r4 = leaf ?
        test.mode == Mode::LeafNullLow ? 0x1234567800000000ull :
        test.mode == Mode::LeafOwnerAlias ?
            0xabcdef0000000000ull | (State - 52u) :
            0xabcdef0000000000ull | Owner :
        0xfedcba9800000022ull;
    const std::uint64_t caller_sp = test.mode == Mode::HubMutation ?
        0x1234567800070000ull : Stack;
    PPCContext context{};
    context.r1.u64 = caller_sp;
    context.r3.u64 = incoming_r3;
    context.r4.u64 = incoming_r4;
    context.r31.u64 = InitialR31;
    context.lr = InitialLR;
    active = &expected;
    if (leaf) __imp__sub_82406A38(context, original.bytes);
    else __imp__sub_823FA008(context, original.bytes);

    owned::FrameRegisters frame{InitialLR, InitialR31};
    std::uint64_t result = 0xdeadbeefcafebabeull;
    active = &actual;
    if (!owned::Apply(test.address, actual.memory, actual, actual,
                      incoming_r3, incoming_r4, caller_sp, frame, result))
        throw std::runtime_error("owned-state entry missing");
    bool same = context.r3.u64 == result && context.lr == frame.lr &&
        context.r31.u64 == frame.r31 && context.r1.u64 == caller_sp &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x80000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
                    recovered.bytes + 0x8330b000u, 0x1000) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL owned-state %08x mode %u r3 %llx/%llx LR %llx/%llx events %zu/%zu\n",
            test.address, static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.lr),
            static_cast<unsigned long long>(frame.lr),
            expected.events.size(), actual.events.size());
        for (std::size_t i = 0, shown = 0; i < 0x80000 && shown < 8; ++i)
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

PPC_FUNC(sub_82486C88)
{
    ctx.r3.u64 = AllocateManagerBuffer(active->memory, *active,
        ctx.r3.u64, ctx.r1.u32);
}
PPC_FUNC(sub_82406A38) { __imp__sub_82406A38(ctx, base); }

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
        Window original, recovered;
        for (const Case& test : Cases)
            if (!Compare(test, original, recovered)) return 1;
        std::array<std::uint8_t, 32> bytes{};
        GuestMemory memory(0, bytes);
        Services untouched(recovered.bytes, Mode::Leaf);
        owned::FrameRegisters frame{11, 22};
        std::uint64_t result = 33;
        if (owned::Apply(0xffffffffu, memory, untouched, untouched,
                         44, 55, 66, frame, result) ||
            frame.lr != 11 || frame.r31 != 22 || result != 33 ||
            !untouched.events.empty() ||
            bytes != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown owned-state entry changed state");
        std::printf("PASS owned-state %zu original-PPC comparisons and 1 unknown\n",
                    std::size(Cases));
        std::puts("LIMIT original two PPC bodies with proven allocation model and explicit destructor callback; dynamic destructor implementation, generic lower ABI, volatile registers/CR and split U64 access excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
