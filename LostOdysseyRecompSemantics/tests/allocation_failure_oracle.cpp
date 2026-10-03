#include "lo_semantics/allocation_failure.h"

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
constexpr GuestAddress Stack = 0xf000;
constexpr GuestAddress Table = 0x83214ff0;
constexpr GuestAddress Handler = 0x832d3ae8;
constexpr GuestAddress HeapGlobal = 0x83245708;
constexpr std::uint32_t CallerLR = 0x82200000;

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve guest window");
        if (!VirtualAlloc(bytes, 0x10000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83214000, 0x2000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83245000, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x832d3000, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest pages");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

enum class Kind { Message, Banner, BugCheck, Handler, ErrorAddress, RawAllocation };
using Event = std::array<std::uint64_t, 4>;
struct Services : AllocationFailureServices
{
    GuestMemory memory;
    Kind kind;
    unsigned variant;
    unsigned allocations = 0;
    unsigned thread_lookups = 0;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Kind operation, unsigned test)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), kind(operation), variant(test) {}
    std::uint64_t OutputErrorMessage(GuestAddress message) override
    {
        events.push_back({1, message, 0});
        if (kind == Kind::Banner && variant == 3)
            memory.WriteU32(Table + 22 * 8 + 4, 0x9900);
        return 0xaabbccdd00000000ull | message;
    }
    std::uint64_t BugCheck(std::uint32_t code) override
    {
        events.push_back({2, code, 0});
        if (kind == Kind::RawAllocation) memory.WriteU32(HeapGlobal, 0x1100);
        return 0x1234567887654321ull;
    }
    std::uint64_t CallNewHandler(GuestAddress handler, std::uint64_t bytes) override
    {
        events.push_back({3, handler, bytes});
        constexpr std::uint64_t results[] = {0, 0x100000000ull, 1, UINT64_MAX, 3};
        memory.WriteU32(Handler, 0); // The active call must not restart or reread.
        if (kind == Kind::RawAllocation) return 1;
        return results[variant];
    }
    std::uint64_t GetThreadData() override
    {
        events.push_back({4, 0, 0});
        if (kind == Kind::RawAllocation) return 0x2000u + 0x100u * thread_lookups++;
        constexpr std::uint64_t results[] = {0, 0x100000000ull,
                                            0x1234567800003000ull, UINT64_MAX - 3};
        return results[variant];
    }
    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags, std::uint64_t bytes)
    {
        events.push_back({5, heap, flags, bytes});
        ++allocations;
        if ((variant == 1 && allocations == 1) || variant == 2) return 0;
        return 0xabcdef0000007000ull;
    }
};
Services* active = nullptr;

struct RawServices : RecoveredRawAllocationServices
{
    Services& services;
    explicit RawServices(Services& state)
        : RecoveredRawAllocationServices(state.memory, state), services(state) {}
    std::uint64_t AllocateHeap(GuestAddress heap, std::uint32_t flags, std::uint64_t bytes) override
    { return services.AllocateHeap(heap, flags, bytes); }
};

void Initialize(std::uint8_t* bytes, Kind kind, unsigned variant)
{
    std::memset(bytes, 0xbd, 0x10000);
    std::memset(bytes + 0x83214000, 0xbd, 0x2000);
    std::memset(bytes + 0x83245000, 0xbd, 0x1000);
    std::memset(bytes + 0x832d3000, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    for (unsigned i = 0; i != 23; ++i)
    {
        memory.WriteU32(Table + i * 8, 100 + i);
        memory.WriteU32(Table + i * 8 + 4, 0x3000 + i * 16);
    }
    if (kind == Kind::Message && variant == 3)
        memory.WriteU32(Table + 22 * 8, 100);
    if (kind == Kind::Banner)
    {
        if (variant != 1) memory.WriteU32(Table, 252);
        if (variant != 2) memory.WriteU32(Table + 22 * 8, 255);
    }
    memory.WriteU32(Handler, variant == 0 ? 0 : variant == 4 ? 3 : 0x82200103);
    if (kind == Kind::RawAllocation)
    {
        memory.WriteU32(HeapGlobal, variant == 3 ? 0 : 0x1100);
        memory.WriteU32(Handler, 0x82200103);
        memory.WriteU32(Handler + 4, variant == 1 ? 1 : 0);
        memory.WriteU32(Table, 252);
        memory.WriteU32(Table + 8, 255);
        memory.WriteU32(Table + 16, 30);
    }
}

bool Test(Kind kind, unsigned variant, Window& original, Window& recovered)
{
    Initialize(original.bytes, kind, variant);
    Initialize(recovered.bytes, kind, variant);
    Services expected(original.bytes, kind, variant);
    Services actual(recovered.bytes, kind, variant);
    const std::uint64_t input = 0xabcdeffe00000000ull |
        (kind == Kind::Message ? (variant == 1 ? 122u : variant == 2 ? 999u : 100u) :
         kind == Kind::RawAllocation && variant == 4 ? 0xfffff001u : 255u);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.r3.u64 = input;
    context.lr = CallerLR;
    active = &expected;
    if (kind == Kind::Banner || kind == Kind::Handler || kind == Kind::ErrorAddress)
    {
        actual.memory.WriteU32(Stack - 8, CallerLR);
        actual.memory.WriteU32(Stack - 96, Stack);
    }
    std::uint64_t result = 0;
    switch (kind)
    {
    case Kind::Message:
        __imp__sub_82B7FC98(context, original.bytes);
        result = ReportRuntimeError(actual.memory, actual, input);
        break;
    case Kind::Banner:
        __imp__sub_82B7FCE0(context, original.bytes);
        result = ReportMissingHeapBanner(actual.memory, actual);
        break;
    case Kind::BugCheck:
        __imp__sub_82B7BF20(context, original.bytes);
        result = TerminateAllocationFailure(actual);
        break;
    case Kind::Handler:
        __imp__sub_82B7FE68(context, original.bytes);
        result = InvokeNewHandler(actual.memory, actual, input);
        break;
    case Kind::ErrorAddress:
        __imp__sub_82B7FD78(context, original.bytes);
        result = GetAllocationErrorAddress(actual);
        break;
    case Kind::RawAllocation:
    {
        __imp__sub_823ACBD0(context, original.bytes);
        RawServices bridge(actual);
        result = AllocateRawMemory(actual.memory, bridge, input);
        break;
    }
    }
    if (context.r3.u64 != result || expected.events != actual.events ||
        // The composed raw-allocation run excludes its nested guest ABI frames.
        std::memcmp(original.bytes, recovered.bytes,
                    kind == Kind::RawAllocation ? Stack - 0x400 : 0x10000) ||
        std::memcmp(original.bytes + 0x83214000, recovered.bytes + 0x83214000, 0x2000) ||
        std::memcmp(original.bytes + 0x83245000, recovered.bytes + 0x83245000, 0x1000) ||
        std::memcmp(original.bytes + 0x832d3000, recovered.bytes + 0x832d3000, 0x1000))
    {
        std::fprintf(stderr, "FAIL allocation-failure kind %u case %u\n",
                     static_cast<unsigned>(kind), variant);
        return false;
    }
    return true;
}
} // namespace

PPC_FUNC(sub_823ADD70)
{ (void)base; ctx.r3.u64 = active->OutputErrorMessage(ctx.r3.u32); }
PPC_FUNC(sub_82B7FC98)
{ __imp__sub_82B7FC98(ctx, base); }
PPC_FUNC(__imp__KeBugCheck)
{ (void)base; ctx.r3.u64 = active->BugCheck(ctx.r3.u32); }
PPC_FUNC(sub_822CA048)
{ (void)base; ctx.r3.u64 = active->GetThreadData(); }
void AllocationFailureIndirect(PPCContext& ctx, std::uint8_t* base, std::uint32_t target)
{ (void)base; ctx.r3.u64 = active->CallNewHandler(target, ctx.r3.u64); }
PPC_FUNC(__savegprlr_28) { (void)ctx; (void)base; }
PPC_FUNC(__restgprlr_28) { (void)ctx; (void)base; }
PPC_FUNC(sub_823ACC98) { __imp__sub_823ACC98(ctx, base); }
PPC_FUNC(sub_823ACCB0)
{ (void)base; ctx.r3.u64 = active->AllocateHeap(ctx.r3.u32, ctx.r4.u32, ctx.r5.u64); }
PPC_FUNC(sub_82B7FCE0) { __imp__sub_82B7FCE0(ctx, base); }
PPC_FUNC(sub_82B7BF20) { __imp__sub_82B7BF20(ctx, base); }
PPC_FUNC(sub_82B7FE68) { __imp__sub_82B7FE68(ctx, base); }
PPC_FUNC(sub_82B7FD78) { __imp__sub_82B7FD78(ctx, base); }

int main()
{
    try
    {
        Window original, recovered;
        constexpr unsigned counts[] = {4, 4, 1, 5, 4, 5};
        for (unsigned kind = 0; kind != 6; ++kind)
            for (unsigned variant = 0; variant != counts[kind]; ++variant)
                if (!Test(static_cast<Kind>(kind), variant, original, recovered)) return 1;
        std::puts("PASS allocation-failure 5 new entries 18 cases +5 raw-allocation compositions");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
