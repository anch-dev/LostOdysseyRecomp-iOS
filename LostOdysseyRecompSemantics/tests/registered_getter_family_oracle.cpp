#include "lo_semantics/registered_getter_family.h"
#include "lo_semantics/registered_inline_constructor.h"

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
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Existing = 0x20000u;
constexpr GuestAddress Replacement = 0x22000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerVtable = 0x30100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr std::uint64_t Allocated = 0x1234567800010000ull;
constexpr std::uint64_t FailedAllocation = 0x1234567800000000ull;
constexpr std::uint64_t InitialR3 = 0xabcdef0000007777ull;

enum class Mode { Existing, FreshMutated, Failure };
struct Entry
{
    GuestAddress address, global, constructor, registration;
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
        if (!bytes || !VirtualAlloc(bytes, 0x70000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83247000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83315000u, 0x5000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit getter oracle guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 4> arguments;
    bool operator==(const Event&) const = default;
};
struct Services final : ManagerFacadeServices,
    registered_constructor_family::RegistrationServices
{
    GuestMemory memory;
    const Entry& entry;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, const Entry& selected, Mode selected_mode)
        : memory(0, std::span<std::uint8_t>(bytes, Space)),
          entry(selected), mode(selected_mode) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t,
        std::uint64_t) override
    { throw std::runtime_error("unexpected release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t bytes, std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        if (method != AllocateMethod || manager != Manager || alignment != 8 ||
            bytes != 376)
            throw std::runtime_error("unexpected allocation arguments");
        return mode == Mode::Failure ? FailedAllocation : Allocated;
    }
    std::uint64_t Register(GuestAddress callback, std::uint64_t r3,
        GuestAddress caller_sp) override
    {
        events.push_back({'R', {callback, r3, caller_sp,
            memory.ReadU32(entry.global)}});
        if (callback != entry.registration || caller_sp != Stack - 96u)
            throw std::runtime_error("unexpected registration arguments");
        if (mode == Mode::FreshMutated)
            memory.WriteU32(entry.global, Replacement);
        return 0x8765432100000000ull | (r3 & 0xffffffffu);
    }
};
Services* active = nullptr;

void Initialize(std::uint8_t* bytes, const Entry& entry, Mode mode)
{
    std::memset(bytes, 0xbd, 0x70000);
    std::memset(bytes + 0x83247000u, 0, 0x1000);
    std::memset(bytes + 0x8330b000u, 0, 0x1000);
    std::memset(bytes + 0x83315000u, 0, 0x5000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4u, AllocateMethod | 3u);
    memory.WriteU32(0x83315ed8u, 0);
    memory.WriteU32(0x83315ef0u, 0x24000u);
    memory.WriteU32(entry.global, mode == Mode::Existing ? Existing : 0);
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
    context.r3.u64 = InitialR3;
    context.r31.u64 = 0x1122334455667788ull;
    active = &expected;
    entry.original(context, original.bytes);
    active = &actual;
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!registered_getter_family::Apply(entry.address, actual.memory,
            actual, actual, InitialR3, Stack, result))
        throw std::runtime_error("missing getter mapping");
    const bool same = context.r3.u64 == result &&
        context.r1.u64 == Stack && context.lr == 0x82200000u &&
        context.r31.u64 == 0x1122334455667788ull &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0 &&
        std::memcmp(original.bytes + 0x83247000u,
            recovered.bytes + 0x83247000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83315000u,
            recovered.bytes + 0x83315000u, 0x5000) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
            recovered.bytes + 0x8330b000u, 0x1000) == 0;
    if (!same)
        std::fprintf(stderr,
            "FAIL registered getter %08x case %u mode %u r3 %llx/%llx events %zu/%zu global %08x/%08x\n",
            entry.address, ordinal, static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(),
            expected.memory.ReadU32(entry.global), actual.memory.ReadU32(entry.global));
    return same;
}
} // namespace

std::uint64_t ConstructorLower(PPCContext& ctx, std::uint32_t address)
{
    if (address == 0x827ce240u)
        return ConstructInlineManagedRegisteredObject(active->memory,
            *active, ctx.r3.u64, ctx.r1.u32);
    std::uint64_t result = 0;
    if (!registered_constructor_family::Apply(address, active->memory,
            *active, *active, ctx.r3.u64, ctx.r1.u32, result))
        throw std::runtime_error("getter constructor missing");
    return result;
}
std::uint64_t RegistrationBoundary(PPCContext& ctx, std::uint32_t target)
{
    return active->Register(target, ctx.r3.u64, ctx.r1.u32);
}
int main()
{
    try
    {
        Window original, recovered;
        unsigned cases = 0;
        for (const Entry& entry : Entries)
        {
            if (!CompareOne(entry, Mode::Existing, cases++, original, recovered) ||
                !CompareOne(entry, Mode::FreshMutated, cases++, original, recovered))
                return 1;
        }
        if (!CompareOne(Entries[0], Mode::Failure, cases++, original, recovered))
            return 1;
        std::printf("PASS registered-getter %zu new entries %u cases\n",
            std::size(Entries), cases);
        std::puts("LIMIT exact cached PPC getter bodies; recovered constructor lower reused; external registration callback and ABI scratch are explicit boundaries");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
