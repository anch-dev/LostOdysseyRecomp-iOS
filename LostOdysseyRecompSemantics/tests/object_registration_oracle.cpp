#include "lo_semantics/object_registration.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Stack = 0x4f000;
constexpr GuestAddress Object = 0x10000;
constexpr GuestAddress Primary = 0x20000;
constexpr GuestAddress OtherPrimary = 0x22000;
constexpr GuestAddress Secondary = 0x24000;
constexpr GuestAddress OtherSecondary = 0x26000;
constexpr GuestAddress Manager = 0x30000;
constexpr GuestAddress ManagerVtable = 0x30100;
constexpr GuestAddress PrimaryVtable = 0x30200;
constexpr GuestAddress ManagerGlobal = 0x8330b608;
constexpr GuestAddress ManagerGlobalPage = ManagerGlobal & ~0xfffu;
constexpr GuestAddress GlobalPage = 0x83315000;
constexpr GuestAddress ListEnabled = 0x83315ed8;
constexpr GuestAddress ListHead = 0x83315ef0;
constexpr GuestAddress PrimaryGlobal = 0x83315f9c;
constexpr GuestAddress SecondaryGlobal = 0x83315f7c;
constexpr GuestAddress AllocateMethod = 0x82345680;
constexpr GuestAddress ReadyMethod = 0x82345690;
constexpr std::uint64_t FullObject = 0x1234567800010000ull;
constexpr std::uint64_t FullOwner = 0xabcdef0000023456ull;

enum class Kind { Base, Initialize, Construct, Register };
struct Case
{
    Kind kind;
    bool enabled = false;
    bool allocation_failure = false;
    bool lazy_manager = false;
    bool first_mismatch = false;
    bool secondary_missing = false;
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve guest window");
        if (!VirtualAlloc(bytes, 0x60000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + ManagerGlobalPage, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + GlobalPage, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest pages");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> arguments;
    bool operator==(const Event&) const = default;
};

struct Services : ManagerFacadeServices, ObjectRegistrationServices
{
    GuestMemory memory;
    const Case& test;
    std::vector<Event> events;
    unsigned get_calls = 0;
    Services(std::uint8_t* bytes, const Case& value)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), test(value) {}

    void Record(char kind, std::uint64_t a = 0, std::uint64_t b = 0,
        std::uint64_t c = 0, std::uint64_t d = 0)
    { events.push_back({kind, {a, b, c, d}}); }

    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    { Record('A', bytes); return Manager; }
    std::uint64_t ConstructPrimary(std::uint64_t value) override
    { Record('C', value); return value; }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected manager fallback"); }
    std::uint64_t CallMethod(GuestAddress method,
        std::uint64_t receiver) override
    { Record('M', method, receiver); return 1; }
    std::uint64_t ReleaseStorage(GuestAddress,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected manager release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress,
        GuestAddress, std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        Record('L', method, manager, bytes, alignment);
        if (method != AllocateMethod || manager != Manager ||
            bytes != 376 || alignment != 8)
            throw std::runtime_error("unexpected manager allocation");
        return test.allocation_failure ? 0x1234567800000000ull : FullObject;
    }

    std::uint64_t GetPrimaryObject() override
    {
        ++get_calls;
        Record('G', get_calls);
        if (test.first_mismatch && get_calls == 1)
        {
            memory.WriteU32(PrimaryGlobal, OtherPrimary);
            return 0xfeed000000027000ull;
        }
        return (test.first_mismatch ? 0xfeed000000028000ull :
            0xfeed000000020000ull);
    }
    std::uint64_t CreateSecondary(std::uint64_t descriptor) override
    {
        Record('S', descriptor);
        return 0xfeed000000024000ull;
    }
    std::uint64_t RegisterSecondary(std::uint64_t incoming_register) override
    {
        Record('T', incoming_register);
        memory.WriteU32(SecondaryGlobal, OtherSecondary);
        if (test.first_mismatch)
            memory.WriteU32(PrimaryGlobal, Primary);
        return 0xfeed0000000000eeull;
    }
    std::uint64_t CallReadyMethod(GuestAddress method,
        std::uint64_t receiver) override
    {
        Record('V', method, receiver);
        if (method != ReadyMethod)
            throw std::runtime_error("unexpected ready method");
        return 0xfeed000000000099ull;
    }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, const Case& test)
{
    std::memset(bytes, 0xbd, 0x60000);
    std::memset(bytes + ManagerGlobalPage, 0xbd, 0x1000);
    std::memset(bytes + GlobalPage, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(ManagerGlobal, test.lazy_manager ? 0 : Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(ManagerVtable + 56u, 0x823456a3);
    memory.WriteU32(ManagerVtable + 60u, 0x823456b3);
    memory.WriteU32(ListEnabled, test.enabled ? 1 : 0);
    memory.WriteU32(ListHead, 0x12340);
    memory.WriteU32(PrimaryGlobal, Primary);
    memory.WriteU32(SecondaryGlobal, test.secondary_missing ? 0 : Secondary);
    memory.WriteU32(Primary, PrimaryVtable);
    memory.WriteU32(OtherPrimary, PrimaryVtable);
    memory.WriteU32(PrimaryVtable + 124u, ReadyMethod | 3u);
    memory.WriteU32(Object, 0xdddddddd);
    memory.WriteU32(Stack + 80u, 0x11223344u);
    memory.WriteU32(Stack + 84u, 0x55667788u);
    memory.WriteU32(Stack + 92u, 0x99aabbccu);
    memory.WriteU32(Stack + 100u, 0xddeeff00u);
    memory.WriteU32(Stack + 108u, 0x10203040u);
}

std::uint64_t CompareOne(const Case& test, unsigned index,
    Window& original, Window& recovered)
{
    Initialize(original.bytes, test);
    Initialize(recovered.bytes, test);
    Services expected(original.bytes, test);
    Services actual(recovered.bytes, test);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000;
    active = &expected;
    std::uint64_t result = 0;
    switch (test.kind)
    {
    case Kind::Base:
        context.r3.u64 = FullObject;
        context.r5.u64 = 376;
        context.r6.u64 = 0x12345678821913eeull;
        context.r7.u64 = FullOwner;
        context.r8.u64 = 0x0102030405060708ull;
        __imp__sub_8240CC58(context, original.bytes);
        result = InitializeRegisteredObjectBase(actual.memory, FullObject,
            376, 0x12345678821913eeull, FullOwner,
            0x0102030405060708ull);
        break;
    case Kind::Initialize:
        context.r3.u64 = FullObject;
        context.r5.u64 = 376;
        context.r6.u64 = 0x10000000;
        context.r7.u64 = 0x12345678000000abu;
        context.r8.u64 = 0xffffffff821913eeull;
        context.r9.u64 = FullOwner;
        context.r10.u64 = 0xffffffff8218c21cull;
        __imp__sub_82410A28(context, original.bytes);
        result = InitializeRegisteredObject(actual.memory, RegisteredObjectInput{
            FullObject, 376, 0x10000000, 0x12345678000000abull,
            0xffffffff821913eeull, FullOwner,
            0xffffffff8218c21cull, Stack});
        break;
    case Kind::Construct:
        context.r3.u64 = FullOwner;
        __imp__sub_82410B90(context, original.bytes);
        result = ConstructRegisteredObject(actual.memory, actual,
            FullOwner, Stack);
        break;
    case Kind::Register:
        context.r3.u64 = 0x1111222233334444ull;
        __imp__sub_82410C48(context, original.bytes);
        result = RegisterObjectGraph(actual.memory, actual);
        break;
    }
    const bool same = context.r3.u64 == result &&
        context.r1.u64 == Stack && context.lr == 0x82200000 &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0 &&
        std::memcmp(original.bytes + ManagerGlobalPage,
                    recovered.bytes + ManagerGlobalPage, 0x1000) == 0 &&
        std::memcmp(original.bytes + GlobalPage,
                    recovered.bytes + GlobalPage, 0x1000) == 0;
    if (!same)
    {
        std::size_t first = 0;
        while (first < Stack - 0x1000u &&
               original.bytes[first] == recovered.bytes[first]) ++first;
        std::fprintf(stderr, "FAIL object-registration case %u kind %u r3 %llx/%llx events %zu/%zu first %zx\n",
            index, static_cast<unsigned>(test.kind),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(), first);
        throw std::runtime_error("object registration mismatch");
    }
    const GuestAddress object_caller_sp = test.kind == Kind::Construct ?
        Stack - 128u : Stack;
    if (test.kind == Kind::Initialize ||
        (test.kind == Kind::Construct && !test.allocation_failure))
    {
        const GuestAddress frame = object_caller_sp - 112u;
        const GuestAddress words[] = {frame + 188u, frame + 80u,
            frame + 84u};
        for (GuestAddress address : words)
            if (std::memcmp(original.bytes + address,
                            recovered.bytes + address, 4) != 0)
                throw std::runtime_error("constructor caller stack mismatch");
        if (original.bytes[frame + 167u] != recovered.bytes[frame + 167u])
            throw std::runtime_error("constructor byte stack mismatch");
        if (test.kind == Kind::Construct)
        {
            const GuestAddress outgoing[] = {object_caller_sp + 80u,
                object_caller_sp + 92u, object_caller_sp + 100u,
                object_caller_sp + 108u};
            for (GuestAddress address : outgoing)
                if (std::memcmp(original.bytes + address,
                                recovered.bytes + address, 4) != 0)
                    throw std::runtime_error("constructor outgoing argument mismatch");
        }
    }
    return result;
}
} // namespace

PPC_FUNC(sub_8240CC58) { __imp__sub_8240CC58(ctx, base); }
PPC_FUNC(sub_82410A28) { __imp__sub_82410A28(ctx, base); }
PPC_FUNC(sub_82410B90) { __imp__sub_82410B90(ctx, base); }
PPC_FUNC(sub_82410C48) { __imp__sub_82410C48(ctx, base); }
PPC_FUNC(sub_82486C88)
{
    (void)base;
    ctx.r3.u64 = AllocateManagerBuffer(active->memory, *active,
        ctx.r3.u64, ctx.r1.u32);
}
PPC_FUNC(sub_824059D8)
{ (void)base; ctx.r3.u64 = active->GetPrimaryObject(); }
PPC_FUNC(sub_82408438)
{ (void)base; ctx.r3.u64 = active->CreateSecondary(ctx.r3.u64); }
PPC_FUNC(sub_824084F0)
{ (void)base; ctx.r3.u64 = active->RegisterSecondary(ctx.r3.u64); }
void ObjectRegistrationIndirect(PPCContext& ctx, std::uint8_t*,
    std::uint32_t method)
{ ctx.r3.u64 = active->CallReadyMethod(method, ctx.r3.u64); }

int main()
{
    try
    {
        const Case cases[] = {
            {Kind::Base}, {Kind::Base, true},
            {Kind::Initialize},
            {Kind::Construct, true},
            {Kind::Construct, true, true},
            {Kind::Construct, false, false, true},
            {Kind::Register},
            {Kind::Register, true, false, false, true},
            {Kind::Register, false, false, false, false, true},
            {Kind::Register, true, false, false, true, true},
        };
        Window original, recovered;
        for (unsigned index = 0; index != std::size(cases); ++index)
            (void)CompareOne(cases[index], index, original, recovered);
        std::puts("PASS object-registration 10 composed original-PPC cases");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
