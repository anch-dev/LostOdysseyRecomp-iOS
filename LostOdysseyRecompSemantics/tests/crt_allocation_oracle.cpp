#include "lo_semantics/crt_allocation.h"

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
constexpr GuestAddress Stack = 0x3f0000;
constexpr GuestAddress Heap = 0x10000;
constexpr GuestAddress Block = 0x100000;
constexpr GuestAddress Sentinel = 0x320000;
constexpr GuestAddress Environment = 0x2000;
constexpr GuestAddress State = 0x3000;
constexpr GuestAddress ErrorRecord = 0x2200;
constexpr GuestAddress Handler = 0x82200103;

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve guest window");
        if (!VirtualAlloc(bytes, 0x400000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83214000, 0x2000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83245000, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x832d3000, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest pages");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

using Event = std::array<std::uint64_t, 5>;
struct Services : CrtAllocationServices
{
    GuestMemory memory;
    unsigned variant;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, unsigned test)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), variant(test) {}

    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::runtime_error("unexpected output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::runtime_error("unexpected bug check"); }
    std::uint64_t CallNewHandler(GuestAddress handler, std::uint64_t bytes) override
    {
        events.push_back({1, handler, bytes, 0, 0});
        if (variant == 3)
        {
            SeedSmallList(14);
            return 1;
        }
        return 0;
    }
    std::uint64_t GetThreadData() override
    {
        events.push_back({2, 0, 0, 0, 0});
        return ErrorRecord;
    }
    void ReportInvalidParameter() override
    { events.push_back({3, 0, 0, 0, 0}); }

    std::uint32_t GetCurrentProcessType() override { return 1; }
    void BugCheck(std::uint32_t, GuestAddress, GuestAddress,
                  std::uint32_t, GuestAddress) override
    { throw std::runtime_error("unexpected heap bug check"); }
    void EnterCriticalSection(GuestAddress address) override
    { events.push_back({4, address, 0, 0, 0}); }
    void LeaveCriticalSection(GuestAddress address) override
    { events.push_back({5, address, 0, 0, 0}); }
    GuestAddress GrowHeap(GuestAddress heap, std::uint32_t bytes) override
    {
        events.push_back({6, heap, bytes, 0, 0});
        return 0;
    }
    std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected virtual allocation"); }
    void RaiseException(GuestAddress) override
    { throw std::runtime_error("unexpected exception"); }
    std::uint32_t CompareMemoryUlong(GuestAddress, std::uint32_t,
                                     std::uint32_t) override
    { throw std::runtime_error("unexpected comparison"); }
    std::int32_t FreeVirtualMemory(GuestAddress base_out,
        GuestAddress size_out, std::uint32_t type, std::uint32_t zero) override
    {
        events.push_back({7, base_out, size_out, type, zero});
        return variant >= 7 ? -1 : 0;
    }
    void DecommitFreeBlock(GuestAddress, GuestAddress,
                           std::uint32_t) override
    { throw std::runtime_error("unexpected decommit"); }

    void SeedSmallList(std::uint32_t units)
    {
        const GuestAddress head = Heap + (units + 48u) * 8u;
        const GuestAddress link = Block + 8u;
        memory.WriteU16(Block, static_cast<std::uint16_t>(units));
        memory.WriteU16(Block + 2u, 0);
        memory.WriteU8(Block + 4u, 0);
        memory.WriteU8(Block + 5u, 0);
        memory.WriteU32(head, link);
        memory.WriteU32(head + 4u, link);
        memory.WriteU32(link, head);
        memory.WriteU32(link + 4u, head);
        const GuestAddress word = Heap + ((units >> 5u) + 88u) * 4u;
        memory.WriteU32(word, memory.ReadU32(word) | (1u << (units & 31u)));
        memory.WriteU32(Heap + 48u, units);
    }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, unsigned variant)
{
    std::memset(bytes, 0xbd, 0x400000);
    std::memset(bytes + 0x83214000, 0xbd, 0x2000);
    std::memset(bytes + 0x83245000, 0xbd, 0x1000);
    std::memset(bytes + 0x832d3000, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(0x83245708, Heap);
    memory.WriteU32(0x832d3aec, variant == 3 ? 1 : 0);
    memory.WriteU32(0x832d3ae8, variant == 3 ? Handler : 0);
    memory.WriteU32(Environment + 256u, State);
    memory.WriteU32(Environment + 336u, 0);
    memory.WriteU32(State + 352u, variant == 8 ? 199 : 0x1234);
    memory.WriteU32(0x832150a8, 0x1234);
    memory.WriteU32(0x832150ac, 55);
    memory.WriteU32(Heap + 20u, 0);
    memory.WriteU32(Heap + 24u, 1); // Heap calls do not acquire a critical section.
    memory.WriteU32(Heap + 28u, 0xffff);
    memory.WriteU32(Heap + 1408u, Sentinel);
    for (std::uint32_t units = 0; units != 128; ++units)
    {
        const GuestAddress head = Heap + (units + 48u) * 8u;
        memory.WriteU32(head, head);
        memory.WriteU32(head + 4u, head);
    }
    for (std::uint32_t i = 0; i != 4; ++i)
        memory.WriteU32(Heap + (88u + i) * 4u, 0);
    if (variant == 0 || variant == 4)
    {
        Services setup(bytes, variant);
        setup.SeedSmallList(variant == 4 ? 2 : 14);
    }
    if (variant >= 6)
    {
        memory.WriteU16(Block, 14);
        memory.WriteU8(Block + 5u, 8); // Virtual allocation branch of FreeHeapBlock.
        const GuestAddress vm_base = Block - 32u;
        memory.WriteU32(vm_base, Sentinel);
        memory.WriteU32(vm_base + 4u, Sentinel);
        memory.WriteU32(Sentinel, vm_base);
        memory.WriteU32(Sentinel + 4u, vm_base);
    }
}

bool Test(unsigned variant, Window& original, Window& recovered)
{
    Initialize(original.bytes, variant);
    Initialize(recovered.bytes, variant);
    Services expected(original.bytes, variant);
    Services actual(recovered.bytes, variant);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.r13.u64 = Environment;
    context.lr = 0x82200000;
    active = &expected;
    std::uint64_t result;
    if (variant <= 4)
    {
        context.r3.u64 = variant == 2 ? 0xffffffffu : variant == 4 ? 0 : 1;
        context.r4.u64 = variant == 2 ? 2 : variant == 4 ? 0 : 196;
        __imp__sub_82B81778(context, original.bytes);
        result = AllocateCrtRecord(actual.memory, actual,
                                   variant == 2 ? 0xffffffffu : variant == 4 ? 0 : 1,
                                   variant == 2 ? 2 : variant == 4 ? 0 : 196,
                                   Stack);
    }
    else
    {
        const std::uint64_t payload = variant == 5 ? 0x1234567800000000ull :
            static_cast<std::uint64_t>(Block + 16u);
        context.r3.u64 = payload;
        __imp__sub_823ADDC0(context, original.bytes);
        result = FreeCrtRecord(actual.memory, actual, payload,
                               Environment, Stack);
    }
    const bool same = context.r3.u64 == result && expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, 0x3e0000) == 0 &&
        std::memcmp(original.bytes + 0x83214000, recovered.bytes + 0x83214000,
                    0x2000) == 0 &&
        std::memcmp(original.bytes + 0x83245000, recovered.bytes + 0x83245000,
                    0x1000) == 0 &&
        std::memcmp(original.bytes + 0x832d3000, recovered.bytes + 0x832d3000,
                    0x1000) == 0 &&
        (variant > 4 || expected.memory.ReadU32(Stack - 32u) ==
                        actual.memory.ReadU32(Stack - 32u));
    if (!same)
    {
        std::fprintf(stderr, "FAIL crt-allocation case %u return %llx/%llx events %zu/%zu\n",
                     variant, static_cast<unsigned long long>(context.r3.u64),
                     static_cast<unsigned long long>(result), expected.events.size(),
                     actual.events.size());
        return false;
    }
    return true;
}
} // namespace

PPC_FUNC(__savegprlr_28) { (void)ctx; (void)base; }
PPC_FUNC(__restgprlr_28) { (void)ctx; (void)base; }
PPC_FUNC(__savegprlr_25) { (void)ctx; (void)base; }
PPC_FUNC(__restgprlr_25) { (void)ctx; (void)base; }
PPC_FUNC(sub_82B816A0) { __imp__sub_82B816A0(ctx, base); }
PPC_FUNC(sub_823ACC98) { __imp__sub_823ACC98(ctx, base); }
PPC_FUNC(sub_82B7FD78) { __imp__sub_82B7FD78(ctx, base); }
PPC_FUNC(sub_82B7FE68) { __imp__sub_82B7FE68(ctx, base); }
PPC_FUNC(sub_82B7FD10) { __imp__sub_82B7FD10(ctx, base); }
PPC_FUNC(sub_822CA100) { __imp__sub_822CA100(ctx, base); }
PPC_FUNC(sub_822CA108) { __imp__sub_822CA108(ctx, base); }
PPC_FUNC(sub_822CA048)
{ (void)base; ctx.r3.u64 = active->GetThreadData(); }
PPC_FUNC(sub_82B7FEC0)
{ (void)base; active->ReportInvalidParameter(); }
PPC_FUNC(sub_823ACCB0)
{
    ctx.r3.u64 = AllocateHeapBlock(active->memory, *active, ctx.r3.u32,
        ctx.r4.u32, ctx.r5.u32, ctx.r1.u32 - 320u);
    (void)base;
}
PPC_FUNC(sub_823ADE28)
{
    ctx.r3.u64 = FreeHeapBlock(active->memory, *active, ctx.r3.u32,
        ctx.r4.u32, ctx.r5.u32, ctx.r1.u32 - 176u);
    (void)base;
}
void CrtAllocationIndirect(PPCContext& ctx, std::uint8_t* base,
                           std::uint32_t function)
{
    (void)base;
    ctx.r3.u64 = active->CallNewHandler(function, ctx.r3.u64);
}

int main()
{
    try
    {
        Window original, recovered;
        for (unsigned variant = 0; variant != 9; ++variant)
            if (!Test(variant, original, recovered)) return 1;
        std::puts("PASS crt-allocation 5 allocation +4 free cases");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
