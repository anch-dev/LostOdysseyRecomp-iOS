#include "lo_semantics/registered_constructor_family.h"
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
using lo::semantic::gpu::registered_constructor_family::RegistrationServices;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerVtable = 0x30100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress RegistrationMode = 0x83315ed8u;
constexpr GuestAddress ListHead = 0x83315ef0u;
constexpr GuestAddress Existing = 0x20000u;
constexpr GuestAddress Replacement = 0x22000u;
constexpr std::uint64_t FullObject = 0x1234567800010000ull;
constexpr std::uint64_t FailedAllocation = 0x1234567800000000ull;
constexpr std::uint64_t FullOwner = 0xabcdef0000023456ull;
constexpr GuestAddress HighPages[] = {
    0x8330b000u, 0x83315000u, 0x83318000u, 0x83319000u
};

enum class Variant { Constructor, Scratch, Singleton };
enum class Mode { Success, Failure, ExistingSingleton, MutateAfterCallback };
struct Entry
{
    GuestAddress address;
    Variant variant;
    GuestAddress singleton;
    GuestAddress callback;
    std::uint32_t frame_size;
    PPCFunc* original;
};
constexpr Entry Entries[] = {
/* ENTRY_TABLE */
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve 4 GiB guest window");
        if (!VirtualAlloc(bytes, 0x70000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit low guest memory");
        for (GuestAddress page : HighPages)
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit high guest page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 5> arguments;
    bool operator==(const Event&) const = default;
};

struct Services final : ManagerFacadeServices, RegistrationServices
{
    GuestMemory memory;
    const Entry& entry;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, const Entry& selected, Mode scenario)
        : memory(0, std::span<std::uint8_t>(bytes, Space)),
          entry(selected), mode(scenario) {}

    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
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
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t bytes, std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment, 0}});
        if (method != AllocateMethod || manager != Manager ||
            bytes != 376 || alignment != 8)
            throw std::runtime_error("unexpected allocation arguments");
        return mode == Mode::Failure ? FailedAllocation : FullObject;
    }
    std::uint64_t Register(GuestAddress callback, std::uint64_t incoming_r3,
        GuestAddress caller_sp) override
    {
        const GuestAddress before = memory.ReadU32(entry.singleton);
        events.push_back({'R', {callback, incoming_r3, caller_sp, before, 0}});
        if (callback != entry.callback ||
            caller_sp != Stack - entry.frame_size)
            throw std::runtime_error("unexpected registration callback");
        if (mode == Mode::MutateAfterCallback)
            memory.WriteU32(entry.singleton, Replacement);
        return 0xfedcba9876543210ull;
    }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, const Entry& entry, Mode mode)
{
    std::memset(bytes, 0xbd, 0x70000);
    for (GuestAddress page : HighPages)
        std::memset(bytes + page, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(RegistrationMode, 0);
    memory.WriteU32(ListHead, 0x24000u);
    if (entry.variant == Variant::Singleton)
        memory.WriteU32(entry.singleton,
            mode == Mode::ExistingSingleton ? Existing : 0);
}

bool CompareOne(const Entry& entry, Mode mode, unsigned ordinal,
    Window& original, Window& recovered)
{
    Initialize(original.bytes, entry, mode);
    Initialize(recovered.bytes, entry, mode);
    Services expected(original.bytes, entry, mode);
    Services actual(recovered.bytes, entry, mode);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000u;
    context.r3.u64 = FullOwner;
    context.r30.u64 = 0x1122334455667788ull;
    context.r31.u64 = 0x2233445566778899ull;
    active = &expected;
    entry.original(context, original.bytes);
    active = &actual;
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!registered_constructor_family::Apply(entry.address, actual.memory,
            actual, actual, FullOwner, Stack, result))
        throw std::runtime_error("missing registered-constructor mapping");

    bool same = context.r3.u64 == result &&
        context.r1.u64 == Stack && context.lr == 0x82200000u &&
        context.r30.u64 == 0x1122334455667788ull &&
        context.r31.u64 == 0x2233445566778899ull &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0;
    for (GuestAddress page : HighPages)
        same &= std::memcmp(original.bytes + page,
                            recovered.bytes + page, 0x1000) == 0;
    const GuestAddress frame = Stack - entry.frame_size;
    for (GuestAddress offset : {80u, 84u, 92u, 100u, 108u})
        same &= expected.memory.ReadU32(frame + offset) ==
                actual.memory.ReadU32(frame + offset);
    if (entry.variant == Variant::Scratch)
        same &= expected.memory.ReadU32(frame + 112u) ==
                actual.memory.ReadU32(frame + 112u);
    if (entry.variant == Variant::Singleton)
    {
        const bool should_register = mode != Mode::ExistingSingleton;
        const bool found = !expected.events.empty() &&
            expected.events.back().kind == 'R';
        same &= should_register == found;
        if (mode == Mode::ExistingSingleton) same &= expected.events.empty();
        if (mode == Mode::MutateAfterCallback) same &= result == Replacement;
    }
    if (!same)
    {
        std::fprintf(stderr,
            "FAIL registered-constructor %08x case %u mode %u r3 %llx/%llx events %zu/%zu\n",
            entry.address, ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size());
        return false;
    }
    return true;
}
} // namespace

std::uint64_t AllocateLower(PPCContext& ctx)
{
    return lo::semantic::gpu::AllocateManagerBuffer(active->memory, *active,
        ctx.r3.u64, ctx.r1.u32);
}

std::uint64_t InitializeLower(PPCContext& ctx)
{
    return lo::semantic::gpu::InitializeRegisteredObject(active->memory,
        lo::semantic::gpu::RegisteredObjectInput{
            ctx.r3.u64, ctx.r5.u64, ctx.r6.u64, ctx.r7.u64,
            ctx.r8.u64, ctx.r9.u64, ctx.r10.u64, ctx.r1.u32});
}

std::uint64_t RegistrationCallback(PPCContext& ctx, std::uint32_t callback)
{
    return active->Register(callback, ctx.r3.u64, ctx.r1.u32);
}

int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        const Entry* first_singleton = nullptr;
        for (const Entry& entry : Entries)
        {
            if (!CompareOne(entry, Mode::Success, cases++, original, recovered))
                return 1;
            if (!CompareOne(entry, Mode::Failure, cases++, original, recovered))
                return 1;
            if (entry.variant == Variant::Singleton && !first_singleton)
                first_singleton = &entry;
        }
        if (!first_singleton ||
            !CompareOne(*first_singleton, Mode::ExistingSingleton, cases++, original, recovered) ||
            !CompareOne(*first_singleton, Mode::MutateAfterCallback, cases++, original, recovered))
            return 1;
        std::printf("PASS registered-constructor %zu new entries %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT original generated PPC bodies from cached evidence; lower allocation/initializer semantics reused; generic ABI scratch and volatile effects excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
