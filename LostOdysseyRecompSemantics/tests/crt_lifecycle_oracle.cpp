#include "lo_semantics/crt_lifecycle.h"

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
constexpr GuestAddress Environment = 0x2000;
constexpr GuestAddress OtherEnvironment = 0x4000;
constexpr GuestAddress State = 0x3000;
constexpr GuestAddress OtherState = 0x5000;
constexpr GuestAddress Getter = 0x82200103;
constexpr GuestAddress Binder = 0x82200203;

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

using Event = std::array<std::uint64_t, 4>;
struct Services : CrtLifecycleServices
{
    GuestMemory memory;
    unsigned variant;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, unsigned test)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), variant(test) {}

    std::uint64_t GetTlsValue(std::uint32_t index) override
    { events.push_back({1, index, 0, 0}); return 0; }
    void SetTlsValue(std::uint32_t index, std::uint64_t value) override
    { events.push_back({2, index, value, 0}); }
    std::uint64_t CallThreadDataGetter(GuestAddress function,
        std::uint64_t context, CrtThreadDataCall& call) override
    {
        events.push_back({3, function, context, call.thread_environment});
        memory.WriteU32(State + 352, 0xeeeeeeee);
        if (variant == 2)
            call.thread_environment = 0xabcdef0000000000ull | OtherEnvironment;
        return 0;
    }
    std::uint64_t BindThreadData(GuestAddress function,
        std::uint64_t context, std::uint64_t data,
        CrtThreadDataCall& call) override
    {
        events.push_back({4, function, context, data});
        if (variant == 2)
            call.thread_environment = 0xabcdef0000000000ull | OtherEnvironment;
        return variant == 1 ? 0 : 1;
    }

    std::uint64_t OutputErrorMessage(GuestAddress) override
    { throw std::runtime_error("unexpected output"); }
    std::uint64_t BugCheck(std::uint32_t) override
    { throw std::runtime_error("unexpected bug check"); }
    std::uint64_t CallNewHandler(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected new handler"); }
    std::uint64_t GetThreadData() override
    { throw std::runtime_error("unexpected error-address thread lookup"); }
    void ReportInvalidParameter() override
    { throw std::runtime_error("unexpected invalid parameter"); }
    std::uint32_t GetCurrentProcessType() override { return 1; }
    void BugCheck(std::uint32_t, GuestAddress, GuestAddress,
        std::uint32_t, GuestAddress) override
    { throw std::runtime_error("unexpected heap bug check"); }
    void EnterCriticalSection(GuestAddress address) override
    { events.push_back({5, address, 0, 0}); }
    void LeaveCriticalSection(GuestAddress address) override
    { events.push_back({6, address, 0, 0}); }
    GuestAddress GrowHeap(GuestAddress, std::uint32_t) override
    { throw std::runtime_error("unexpected heap growth"); }
    std::int32_t AllocateVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected VM allocation"); }
    void RaiseException(GuestAddress) override
    { throw std::runtime_error("unexpected exception"); }
    std::uint32_t CompareMemoryUlong(GuestAddress, std::uint32_t,
        std::uint32_t) override
    { throw std::runtime_error("unexpected comparison"); }
    std::int32_t FreeVirtualMemory(GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected VM release"); }
    void DecommitFreeBlock(GuestAddress, GuestAddress, std::uint32_t) override
    { throw std::runtime_error("unexpected decommit"); }
};

Services* active = nullptr;
struct SavedGprs
{
    unsigned first;
    std::array<std::uint64_t, 7> registers;
    std::uint64_t return_address;
};
std::vector<SavedGprs> saved_gprs;

void SaveGprs(PPCContext& ctx, unsigned first)
{
    SavedGprs save{first, {ctx.r25.u64, ctx.r26.u64, ctx.r27.u64,
        ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64}, ctx.r12.u64};
    saved_gprs.push_back(save);
}

void RestoreGprs(PPCContext& ctx, unsigned first)
{
    if (saved_gprs.empty() || saved_gprs.back().first != first)
        throw std::runtime_error("unmatched nonvolatile register restore");
    const SavedGprs save = saved_gprs.back();
    saved_gprs.pop_back();
    PPCRegister* registers[] = {&ctx.r25, &ctx.r26, &ctx.r27, &ctx.r28,
        &ctx.r29, &ctx.r30, &ctx.r31};
    for (unsigned register_number = first; register_number <= 31; ++register_number)
        registers[register_number - 25]->u64 = save.registers[register_number - 25];
    ctx.lr = save.return_address;
}

void Initialize(std::uint8_t* bytes)
{
    std::memset(bytes, 0xbd, 0x400000);
    std::memset(bytes + 0x83214000, 0xbd, 0x2000);
    std::memset(bytes + 0x83245000, 0xbd, 0x1000);
    std::memset(bytes + 0x832d3000, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(Environment + 256, State);
    memory.WriteU32(Environment + 336, 0);
    memory.WriteU32(OtherEnvironment + 256, OtherState);
    memory.WriteU32(OtherEnvironment + 336, 0);
    memory.WriteU32(State + 332, 0x11223344);
    memory.WriteU32(State + 352, 0x12345678);
    memory.WriteU32(OtherState + 332, 0x55667788);
    memory.WriteU32(OtherState + 352, 0xabcdef01);
    memory.WriteU32(0x83214d74, 0x1234);
    memory.WriteU32(0x83214d78, 7);
    memory.WriteU32(0x832d3adc, Getter);
    memory.WriteU32(0x832d3ae0, Binder);
    memory.WriteU32(0x83245708, Heap);
    memory.WriteU32(0x832d3aec, 0);
    memory.WriteU32(Heap + 20, 0);
    memory.WriteU32(Heap + 24, 1);
    memory.WriteU32(Heap + 28, 0xffff);
    const std::uint32_t units = 14;
    const GuestAddress head = Heap + (units + 48u) * 8u;
    const GuestAddress link = Block + 8u;
    memory.WriteU16(Block, units);
    memory.WriteU16(Block + 2u, 0);
    memory.WriteU8(Block + 4u, 0);
    memory.WriteU8(Block + 5u, 0);
    memory.WriteU32(head, link);
    memory.WriteU32(head + 4u, link);
    memory.WriteU32(link, head);
    memory.WriteU32(link + 4u, head);
    memory.WriteU32(Heap + ((units >> 5u) + 88u) * 4u, 1u << (units & 31u));
    memory.WriteU32(Heap + 48u, units);
    memory.WriteU32(Heap + 1408u, 0x320000);
}

bool Test(unsigned variant, Window& original, Window& recovered)
{
    Initialize(original.bytes);
    Initialize(recovered.bytes);
    Services expected(original.bytes, variant);
    Services actual(recovered.bytes, variant);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.r13.u64 = 0x1234567800000000ull | Environment;
    context.lr = 0x82200000;
    active = &expected;
    saved_gprs.clear();
    __imp__sub_822CA048(context, original.bytes);
    if (!saved_gprs.empty()) throw std::runtime_error("unrestored nonvolatile registers");
    CrtThreadDataCall call{0x1234567800000000ull | Environment};
    const std::uint64_t result = GetCrtThreadDataComposed(actual.memory,
        actual, call, Stack);
    const bool same = context.r3.u64 == result &&
        context.r13.u64 == call.thread_environment &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83214000,
                    recovered.bytes + 0x83214000, 0x2000) == 0 &&
        std::memcmp(original.bytes + 0x83245000,
                    recovered.bytes + 0x83245000, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x832d3000,
                    recovered.bytes + 0x832d3000, 0x1000) == 0 &&
        std::memcmp(original.bytes + Stack - 112u - 32u,
                    recovered.bytes + Stack - 112u - 32u, 4) == 0;
    if (!same)
    {
        std::fprintf(stderr, "FAIL crt-lifecycle case %u return %llx/%llx r13 %llx/%llx events %zu/%zu\n",
            variant, static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(context.r13.u64),
            static_cast<unsigned long long>(call.thread_environment),
            expected.events.size(), actual.events.size());
        for (GuestAddress address = 0; address < Stack - 0x1000u; ++address)
            if (original.bytes[address] != recovered.bytes[address])
            {
                std::fprintf(stderr, "first low-memory difference %x: %02x/%02x\n",
                    address, original.bytes[address], recovered.bytes[address]);
                break;
            }
        for (std::size_t index = 0; index < expected.events.size(); ++index)
            std::fprintf(stderr, "event %zu: %llu %llx %llx %llx / %llu %llx %llx %llx\n",
                index, static_cast<unsigned long long>(expected.events[index][0]),
                static_cast<unsigned long long>(expected.events[index][1]),
                static_cast<unsigned long long>(expected.events[index][2]),
                static_cast<unsigned long long>(expected.events[index][3]),
                static_cast<unsigned long long>(actual.events[index][0]),
                static_cast<unsigned long long>(actual.events[index][1]),
                static_cast<unsigned long long>(actual.events[index][2]),
                static_cast<unsigned long long>(actual.events[index][3]));
        return false;
    }
    return true;
}
} // namespace

PPC_FUNC(__savegprlr_29) { (void)base; SaveGprs(ctx, 29); }
PPC_FUNC(__restgprlr_29) { (void)base; RestoreGprs(ctx, 29); }
PPC_FUNC(__savegprlr_28) { (void)base; SaveGprs(ctx, 28); }
PPC_FUNC(__restgprlr_28) { (void)base; RestoreGprs(ctx, 28); }
PPC_FUNC(__savegprlr_25) { (void)base; SaveGprs(ctx, 25); }
PPC_FUNC(__restgprlr_25) { (void)base; RestoreGprs(ctx, 25); }
PPC_FUNC(sub_822CA100) { __imp__sub_822CA100(ctx, base); }
PPC_FUNC(sub_822CA108) { __imp__sub_822CA108(ctx, base); }
PPC_FUNC(sub_822CA128) { __imp__sub_822CA128(ctx, base); }
PPC_FUNC(sub_822CA180) { __imp__sub_822CA180(ctx, base); }
PPC_FUNC(sub_822CA188) { __imp__sub_822CA188(ctx, base); }
PPC_FUNC(sub_82290AA8) { __imp__sub_82290AA8(ctx, base); }
PPC_FUNC(sub_82B81778) { __imp__sub_82B81778(ctx, base); }
PPC_FUNC(sub_82B816A0) { __imp__sub_82B816A0(ctx, base); }
PPC_FUNC(sub_823ADDC0) { __imp__sub_823ADDC0(ctx, base); }
PPC_FUNC(sub_823ACC98) { __imp__sub_823ACC98(ctx, base); }
PPC_FUNC(sub_82B7FD78) { __imp__sub_82B7FD78(ctx, base); }
PPC_FUNC(sub_82B7FE68) { __imp__sub_82B7FE68(ctx, base); }
PPC_FUNC(sub_82B7FD10) { __imp__sub_82B7FD10(ctx, base); }
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
PPC_FUNC(__imp__KeTlsGetValue)
{ (void)base; ctx.r3.u64 = active->GetTlsValue(ctx.r3.u32); }
PPC_FUNC(__imp__KeTlsSetValue)
{ (void)base; active->SetTlsValue(ctx.r3.u32, ctx.r4.u64); }
void CrtLifecycleIndirect(PPCContext& ctx, std::uint8_t*, std::uint32_t function)
{
    CrtThreadDataCall call{ctx.r13.u64};
    if (function == (Getter & ~3u))
        ctx.r3.u64 = active->CallThreadDataGetter(function, ctx.r3.u64, call);
    else if (function == (Binder & ~3u))
        ctx.r3.u64 = active->BindThreadData(function, ctx.r3.u64,
                                             ctx.r4.u64, call);
    else
        ctx.r3.u64 = active->CallNewHandler(function, ctx.r3.u64);
    ctx.r13.u64 = call.thread_environment;
}

int main()
{
    try
    {
        Window original, recovered;
        for (unsigned variant = 0; variant != 3; ++variant)
            if (!Test(variant, original, recovered)) return 1;
        std::puts("PASS crt-lifecycle 3 composed original-PPC paths");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
