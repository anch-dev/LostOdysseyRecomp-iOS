#include "lo_semantics/pointer_vector.h"

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
constexpr GuestAddress Globals = 0x83315000u;
constexpr GuestAddress SectionGlobal = 0x83315fd4u;
constexpr GuestAddress ManagerGlobal = 0x83315fd8u;
constexpr GuestAddress Object = 0x1000u;
constexpr GuestAddress Stack = 0xf000u;
constexpr GuestAddress Frame = Stack - 144u;
constexpr std::uint64_t LeaveResult = 0xabcdef0011223344ull;

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x10000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + Globals, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("guest window allocation failed");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

using Event = std::array<std::uint64_t, 4>;
struct Services : ManagerLockServices, FallbackResizeServices
{
    GuestMemory memory;
    unsigned scenario;
    unsigned released = 0;
    std::vector<Event> events;

    Services(std::uint8_t* bytes, unsigned which)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), scenario(which) {}

    std::uint64_t TryEnterCriticalSection(std::uint64_t section) override
    {
        events.push_back({1, section, 0, 0});
        if (scenario == 9) memory.WriteU32(Object + 12, 2);
        return 1;
    }
    std::uint64_t LeaveCriticalSection(std::uint64_t section) override
    {
        events.push_back({3, section, memory.ReadU32(Object + 12), 0});
        return LeaveResult;
    }
    std::uint64_t ReleaseThroughManager(GuestAddress method,
        std::uint64_t manager, std::uint64_t pointer) override
    {
        events.push_back({2, method, manager, pointer});
        ++released;
        if (released == 1)
        {
            if (scenario == 4) memory.WriteU32(Object + 12, 0);
            if (scenario == 5) memory.WriteU32(Object + 12, 3);
            if (scenario == 6) memory.WriteU32(ManagerGlobal, 0x4100);
            if (scenario == 7) memory.WriteU32(Object, 0x3100);
            if (scenario == 8) memory.WriteU32(Frame + 80, 0x6100);
        }
        return 0xfee1000000000000ull + released;
    }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected CallMethod"); }
    std::uint64_t GrowPointerVector(std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected recursive flush"); }
    std::uint64_t ReallocateThroughManager(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected reallocation"); }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, unsigned scenario)
{
    std::memset(bytes, 0xbd, 0x10000);
    std::memset(bytes + Globals, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    const std::uint32_t counts[] = {0, 0xffffffffu, 1, 3, 3, 1, 3, 3, 3, 0};
    memory.WriteU32(SectionGlobal, 0x6000);
    memory.WriteU32(ManagerGlobal, 0x4000);
    memory.WriteU32(Object, 0x3000);
    memory.WriteU32(Object + 12, counts[scenario]);
    memory.WriteU32(Object + 16, 4);
    memory.WriteU32(0x4000, 0x5000);
    memory.WriteU32(0x4100, 0x5100);
    memory.WriteU32(0x500c, 0x82200103);
    memory.WriteU32(0x510c, 0x82200202);
    for (unsigned i = 0; i != 4; ++i)
    {
        memory.WriteU32(0x3000 + i * 4, 0x7000 + i * 16);
        memory.WriteU32(0x3100 + i * 4, 0x8000 + i * 16);
    }
}
} // namespace

// These boundaries isolate the new flush logic. Existing lock retry behavior
// has its own comparison suite; ABI saves and volatile registers are excluded.
PPC_FUNC(__savegprlr_27) { (void)ctx; (void)base; }
PPC_FUNC(__restgprlr_27) { (void)ctx; (void)base; }
PPC_FUNC(sub_822958F8)
{
    (void)base;
    const auto holder = ctx.r3.u64;
    active->memory.WriteU32(ctx.r3.u32, ctx.r4.u32);
    (void)active->TryEnterCriticalSection(ctx.r4.u64 + 4);
    ctx.r3.u64 = holder;
}
PPC_FUNC(__imp__RtlLeaveCriticalSection)
{
    (void)base;
    ctx.r3.u64 = active->LeaveCriticalSection(ctx.r3.u64);
}
void PointerVectorIndirect(PPCContext& ctx, std::uint8_t* base, std::uint32_t method)
{
    (void)base;
    ctx.r3.u64 = active->ReleaseThroughManager(method, ctx.r3.u64, ctx.r4.u64);
}

int main()
{
    try
    {
        Window original;
        Window recovered;
        for (unsigned scenario = 0; scenario != 10; ++scenario)
        {
            Initialize(original.bytes, scenario);
            Initialize(recovered.bytes, scenario);
            Services expected(original.bytes, scenario);
            Services actual(recovered.bytes, scenario);
            PPCContext context{};
            context.r1.u64 = Stack;
            context.r3.u64 = 0xabcdef0000000000ull | Object;
            active = &expected;
            __imp__sub_827C4FA0(context, original.bytes);
            actual.memory.WriteU32(Frame, Stack); // Adapter supplies the backchain.
            const auto result = FlushPointerVector(actual.memory, actual, actual,
                0xabcdef0000000000ull | Object, Frame);
            if (context.r3.u64 != result || expected.events != actual.events ||
                std::memcmp(original.bytes, recovered.bytes, 0x10000) != 0 ||
                std::memcmp(original.bytes + Globals, recovered.bytes + Globals, 0x1000) != 0)
            {
                std::fprintf(stderr, "FAIL pointer-vector scenario %u\n", scenario);
                return 1;
            }
        }
        std::puts("PASS pointer-vector 1 entry 10 cases: return, memory, ordered calls");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
