#include "lo_semantics/object_startup.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Vtable = 0x10100u;
constexpr GuestAddress Stack = 0x2f000u;
constexpr GuestAddress PrepareMethod = 0x82400080u;
constexpr GuestAddress PollMethod = 0x82400090u;
constexpr GuestAddress ReplacementPollMethod = 0x824000a0u;
constexpr GuestAddress CompleteMethod = 0x824000b0u;
constexpr std::uint64_t ObjectRegister = 0x1234567800010000ull;

enum class Kind { Prepare, Complete, Start };
struct Case
{
    Kind kind;
    std::uint64_t flags = 0x1234567800020020ull;
    std::uint32_t state = 0;
    unsigned failures = 0;
    bool mutate_poll_slot = false;
    bool finish_while_waiting = false;
};

struct Event
{
    char kind;
    std::uint64_t receiver;
    GuestAddress method;
    std::uint64_t flags;
    bool operator==(const Event&) const = default;
};

std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
           memory.ReadU32(address + 4u);
}

void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

struct Services final : ObjectStartupServices
{
    GuestMemory memory;
    const Case& test;
    std::vector<Event> events;
    unsigned polls = 0;

    Services(std::vector<std::uint8_t>& bytes, const Case& value)
        : memory(0, bytes), test(value) {}

    std::uint64_t CallMethod(GuestAddress method,
        std::uint64_t receiver) override
    {
        events.push_back({'M', receiver, method, Read64(memory, Object + 8u)});
        if (receiver != ObjectRegister)
            throw std::runtime_error("wrong object register");
        if (method == PollMethod || method == ReplacementPollMethod)
        {
            if (method == PollMethod && test.mutate_poll_slot)
                memory.WriteU32(Vtable + 36u, ReplacementPollMethod | 3u);
            return polls++ < test.failures ? 0xabcdef0000000000ull :
                   0xabcdef0000000001ull;
        }
        if (method != PrepareMethod && method != CompleteMethod)
            throw std::runtime_error("wrong object method");
        return 0xfedcba9876543210ull;
    }

    std::uint64_t WaitForRetry(std::uint64_t argument) override
    {
        events.push_back({'W', argument, 0, Read64(memory, Object + 8u)});
        if (argument != 0) throw std::runtime_error("wrong retry argument");
        if (test.finish_while_waiting)
            Write64(memory, Object + 8u,
                Read64(memory, Object + 8u) | 0x10000ull);
        return 0xdeadbeef01234567ull;
    }
};

Services* active = nullptr;

void Initialize(std::vector<std::uint8_t>& bytes, const Case& test)
{
    std::fill(bytes.begin(), bytes.end(), 0xbdu);
    GuestMemory memory(0, bytes);
    memory.WriteU32(Object, Vtable);
    memory.WriteU32(Object + 4u, test.state);
    Write64(memory, Object + 8u, test.flags);
    memory.WriteU32(Vtable + 32u, PrepareMethod | 3u);
    memory.WriteU32(Vtable + 36u, PollMethod | 3u);
    memory.WriteU32(Vtable + 40u, CompleteMethod | 3u);
}

bool TestCase(const Case& test, unsigned index)
{
    std::vector<std::uint8_t> original(0x30000), recovered(0x30000);
    Initialize(original, test);
    Initialize(recovered, test);
    Services expected(original, test), actual(recovered, test);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x81234567u;
    context.r3.u64 = ObjectRegister;
    active = &expected;
    switch (test.kind)
    {
    case Kind::Prepare: __imp__sub_823F8980(context, original.data()); break;
    case Kind::Complete: __imp__sub_823F89F8(context, original.data()); break;
    case Kind::Start: __imp__sub_823F8A68(context, original.data()); break;
    }
    std::uint64_t result = 0;
    switch (test.kind)
    {
    case Kind::Prepare:
        result = PrepareObject(actual.memory, actual, ObjectRegister); break;
    case Kind::Complete:
        result = CompleteObjectStartup(actual.memory, actual, ObjectRegister); break;
    case Kind::Start:
        result = StartObject(actual.memory, actual, ObjectRegister); break;
    }
    const bool same = context.r3.u64 == result &&
        context.r1.u64 == Stack && context.lr == 0x81234567u &&
        expected.events == actual.events &&
        std::memcmp(original.data() + Object, recovered.data() + Object, 0x200) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL object-startup case %u kind %u result %llx/%llx events %zu/%zu flags %llx/%llx\n",
            index, static_cast<unsigned>(test.kind),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(Read64(expected.memory, Object + 8u)),
            static_cast<unsigned long long>(Read64(actual.memory, Object + 8u)));
        return false;
    }
    return true;
}

constexpr std::size_t GuestSpace = std::size_t{1} << 32;
constexpr GuestAddress PrimaryGlobal = 0x83315f9cu;
constexpr GuestAddress SecondaryGlobal = 0x83315f7cu;
constexpr GuestAddress ReadyGlobal = 0x83315ed8u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress Manager = 0x20000u;
constexpr GuestAddress ManagerVtable = 0x20100u;
constexpr GuestAddress AllocateMethod = 0x82456780u;
constexpr GuestAddress ReadyMethod = 0x82456790u;
constexpr GuestAddress Secondary = 0x30000u;
constexpr GuestAddress ReplacedPrimary = 0x12000u;

struct GlobalCase
{
    bool lazy;
    bool ready;
    bool mutate_primary;
};

struct SparseWindow
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, GuestSpace, MEM_RESERVE, PAGE_NOACCESS));
    SparseWindow()
    {
        if (bytes == nullptr ||
            VirtualAlloc(bytes, 0x40000, MEM_COMMIT, PAGE_READWRITE) == nullptr ||
            VirtualAlloc(bytes + 0x82005000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) == nullptr ||
            VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) == nullptr ||
            VirtualAlloc(bytes + 0x83315000u, 0x2000, MEM_COMMIT, PAGE_READWRITE) == nullptr)
            throw std::runtime_error("commit sparse object window");
    }
    ~SparseWindow() { if (bytes != nullptr) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct GlobalServices final : ManagerFacadeServices, ObjectRegistrationServices
{
    GuestMemory memory;
    const GlobalCase& test;
    std::vector<Event> events;

    GlobalServices(std::uint8_t* bytes, const GlobalCase& value)
        : memory(0, std::span<std::uint8_t>(bytes, GuestSpace)), test(value) {}

    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager init"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected storage release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected storage resize"); }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        if (method != AllocateMethod || manager != Manager ||
            bytes != 376 || alignment != 8)
            throw std::runtime_error("unexpected registered allocation");
        events.push_back({'A', bytes, method, manager});
        return ObjectRegister;
    }
    std::uint64_t GetPrimaryObject() override
    {
        events.push_back({'P', 0, 0, memory.ReadU32(PrimaryGlobal)});
        return Object;
    }
    std::uint64_t CreateSecondary(std::uint64_t) override
    { throw std::runtime_error("unexpected secondary creation"); }
    std::uint64_t RegisterSecondary(std::uint64_t) override
    { throw std::runtime_error("unexpected secondary registration"); }
    std::uint64_t CallReadyMethod(GuestAddress method,
        std::uint64_t receiver) override
    {
        if (method != ReadyMethod || receiver != Object)
            throw std::runtime_error("unexpected ready method");
        events.push_back({'R', receiver, method, memory.ReadU32(PrimaryGlobal)});
        if (test.mutate_primary)
            memory.WriteU32(PrimaryGlobal, ReplacedPrimary);
        return 0xfeedface12345678ull;
    }
};

GlobalServices* global_active = nullptr;

void InitializeGlobal(std::uint8_t* bytes, const GlobalCase& test)
{
    std::memset(bytes, 0xbd, 0x40000);
    std::memset(bytes + 0x82005000u, 0xbd, 0x1000);
    std::memset(bytes + 0x8330b000u, 0xbd, 0x1000);
    std::memset(bytes + 0x83315000u, 0xbd, 0x2000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, GuestSpace));
    memory.WriteU32(PrimaryGlobal, test.lazy ? 0 : ReplacedPrimary);
    memory.WriteU32(SecondaryGlobal, Secondary);
    memory.WriteU32(ReadyGlobal, test.ready ? 1 : 0);
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(0x82005160u + 124u, ReadyMethod | 3u);
}

bool TestGlobal(const GlobalCase& test, unsigned index,
    SparseWindow& original, SparseWindow& recovered)
{
    InitializeGlobal(original.bytes, test);
    InitializeGlobal(recovered.bytes, test);
    GlobalServices expected(original.bytes, test), actual(recovered.bytes, test);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x81234567u;
    context.r3.u64 = 0x9988776655443322ull;
    global_active = &expected;
    __imp__sub_82406B00(context, original.bytes);
    const std::uint64_t result = GetPrimaryRegisteredObject(actual.memory,
        actual, actual, Stack);
    const bool same = context.r3.u64 == result &&
        context.r1.u64 == Stack && context.lr == 0x81234567u &&
        expected.events == actual.events &&
        std::memcmp(original.bytes + Object, recovered.bytes + Object, 0x200) == 0 &&
        std::memcmp(original.bytes + 0x83315000u,
                    recovered.bytes + 0x83315000u, 0x2000) == 0;
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL primary-global case %u result %llx/%llx events %zu/%zu global %x/%x\n",
            index, static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(),
            expected.memory.ReadU32(PrimaryGlobal),
            actual.memory.ReadU32(PrimaryGlobal));
        return false;
    }
    return true;
}
} // namespace

PPC_FUNC(sub_823F8980) { __imp__sub_823F8980(ctx, base); }
PPC_FUNC(sub_823F89F8) { __imp__sub_823F89F8(ctx, base); }
PPC_FUNC(sub_827CA048)
{
    (void)base;
    ctx.r4.s64 = 0;
    ctx.r3.u64 = active->WaitForRetry(ctx.r3.u64);
}
void ObjectStartupIndirect(PPCContext& ctx, std::uint8_t*, std::uint32_t method)
{
    ctx.r3.u64 = active->CallMethod(method, ctx.r3.u64);
}
PPC_FUNC(sub_82410B90)
{
    (void)base;
    ctx.r3.u64 = ConstructRegisteredObject(global_active->memory,
        *global_active, ctx.r3.u64, ctx.r1.u32);
}
PPC_FUNC(sub_82410C48)
{
    (void)base;
    ctx.r3.u64 = RegisterObjectGraph(global_active->memory, *global_active);
}

int main()
{
    try
    {
        const Case cases[] = {
            {Kind::Prepare},
            {Kind::Prepare, 0x9876543200008000ull},
            {Kind::Prepare, 0, 0xffffffffu},
            {Kind::Complete},
            {Kind::Complete, 0x9876543200010000ull},
            {Kind::Complete, 0, 0xffffffffu},
            {Kind::Start},
            {Kind::Start, 0x9876543200010000ull},
            {Kind::Start, 0x1234567800020020ull, 0, 2, true},
            {Kind::Start, 0, 0xffffffffu},
            {Kind::Start, 0, 0, 1, false, true},
        };
        for (unsigned index = 0; index != std::size(cases); ++index)
            if (!TestCase(cases[index], index)) return 1;
        const GlobalCase global_cases[] = {
            {false, false, false}, {true, false, false},
            {true, true, false}, {true, true, true},
        };
        SparseWindow global_original, global_recovered;
        for (unsigned index = 0; index != std::size(global_cases); ++index)
            if (!TestGlobal(global_cases[index], index,
                    global_original, global_recovered)) return 1;
        std::puts("PASS object-startup 15 original-PPC cases");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
