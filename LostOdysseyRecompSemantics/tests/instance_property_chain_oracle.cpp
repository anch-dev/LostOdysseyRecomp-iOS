#include "lo_semantics/instance_property_chain_family.h"

#include "lo_semantics/memory_move.h"
#include "lo_semantics/registered_metadata_string.h"

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
namespace chain = lo::semantic::gpu::instance_property_chain_family;
namespace string = lo::semantic::gpu::registered_metadata_string;

constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x12000u;
constexpr GuestAddress Stack = 0x18000u;
constexpr GuestAddress Manager = 0x8000u;
constexpr GuestAddress Vtable = 0x8100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress ResizeMethod = 0x82340000u;
constexpr GuestAddress ReleaseMethod = 0x82340010u;
constexpr GuestAddress StringSource = 0x821a83d0u;
constexpr GuestAddress StringHeaders[] = {
    0x8336a894u, 0x8336a8d0u, 0x8336a8acu, 0x8336a8dcu,
};
constexpr GuestAddress StringData[] = {
    0x28000u, 0x28020u, 0x28040u, 0x28060u,
};

enum class Mode { Ordinary, Null, StageAlias };
struct Case
{
    GuestAddress address;
    PPCFunc* original;
    Mode mode;
};
constexpr Case Cases[] = {
/* CASE_TABLE */
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x50000, MEM_COMMIT,
                PAGE_READWRITE))
            throw std::runtime_error("reserve property-chain low guest window");
        for (GuestAddress page : {0x821a8000u, 0x8330b000u,
                                  0x83318000u, 0x8336a000u})
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT,
                    PAGE_READWRITE))
                throw std::runtime_error("commit property-chain guest page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 5> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices, ManagerFacadeServices
{
    unsigned next_storage = 0;
    std::vector<Event> events;
    void InitializeManager() override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t storage) override
    {
        events.push_back({'L', {method, manager, storage, 0, 0}});
        if (method != ReleaseMethod || manager != Manager)
            throw std::runtime_error("wrong release callback");
        return storage;
    }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected allocation callback"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'R', {method, manager, storage, bytes, argument}});
        if (method != ResizeMethod || manager != Manager || argument != 8)
            throw std::runtime_error("wrong resize callback");
        return 0x30000u + 0x100u * next_storage++;
    }
};

Services* active = nullptr;

void Initialize(Window& window)
{
    std::memset(window.bytes, 0xbd, 0x50000);
    for (GuestAddress page : {0x821a8000u, 0x8330b000u,
                              0x83318000u, 0x8336a000u})
        std::memset(window.bytes + page, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 12u, ReleaseMethod | 3u);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(0x833180acu, 0x44556677u);
    for (unsigned i = 0; i < std::size(StringHeaders); ++i)
    {
        memory.WriteU32(StringHeaders[i], StringData[i]);
        memory.WriteU32(StringHeaders[i] + 4u, 2);
        memory.WriteU32(StringHeaders[i] + 8u, 2);
        memory.WriteU16(StringData[i], static_cast<std::uint16_t>('A' + i));
        memory.WriteU16(StringData[i] + 2u, 0);
    }
    memory.WriteU16(StringSource, 'S');
    memory.WriteU16(StringSource + 2u, 0);
}

bool SameMemory(const Window& first, const Window& second)
{
    if (std::memcmp(first.bytes, second.bytes, 0x50000) != 0)
        return false;
    for (GuestAddress page : {0x821a8000u, 0x8330b000u,
                              0x83318000u, 0x8336a000u})
        if (std::memcmp(first.bytes + page, second.bytes + page, 0x1000))
            return false;
    return true;
}

bool Compare(const Case& test)
{
    Window original, recovered;
    Initialize(original);
    Initialize(recovered);
    const GuestAddress object = test.mode == Mode::Null ? 0 :
        test.mode == Mode::StageAlias ? Stack - 212u : Object;
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0x1234567800000000ull | object;
    raw.r4.u64 = 0xdeadbeef00005678ull;
    raw.lr = 0x1122334455667788ull;
    raw.r28.u64 = 0x8877665544332211ull;
    raw.r29.u64 = 0x99aabbccddeeff00ull;
    raw.r30.u64 = 0x1020304050607080ull;
    raw.r31.u64 = 0xa0b0c0d0e0f00112ull;
    PPCContext state = raw;
    Services expected;
    active = &expected;
    test.original(raw, original.bytes);

    Services actual;
    GuestMemory memory(0, std::span<std::uint8_t>(recovered.bytes, Space));
    chain::FrameRegisters frame{state.lr, state.r28.u64, state.r29.u64,
                                state.r30.u64, state.r31.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!chain::Apply(test.address, memory, actual, actual,
                      state.r3.u64, state.r4.u64, Stack, frame, result))
        throw std::runtime_error("property chain entry unmapped");
    state.r3.u64 = result;
    state.lr = frame.lr;
    state.r28.u64 = frame.r28;
    state.r29.u64 = frame.r29;
    state.r30.u64 = frame.r30;
    state.r31.u64 = frame.r31;
    const bool same = raw.r1.u64 == state.r1.u64 &&
        raw.r3.u64 == state.r3.u64 && raw.lr == state.lr &&
        raw.r28.u64 == state.r28.u64 && raw.r29.u64 == state.r29.u64 &&
        raw.r30.u64 == state.r30.u64 && raw.r31.u64 == state.r31.u64 &&
        expected.events == actual.events && SameMemory(original, recovered);
    if (!same)
        std::fprintf(stderr,
            "FAIL property chain %08x mode %d raw-r3 %016llx vs %016llx "
            "raw-r31 %016llx vs %016llx events %zu/%zu\n", test.address,
            static_cast<int>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(state.r3.u64),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(state.r31.u64),
            expected.events.size(), actual.events.size());
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_28)
{
    PPC_STORE_U64(ctx.r1.u32 - 40u, ctx.r28.u64);
    PPC_STORE_U64(ctx.r1.u32 - 32u, ctx.r29.u64);
    PPC_STORE_U64(ctx.r1.u32 - 24u, ctx.r30.u64);
    PPC_STORE_U64(ctx.r1.u32 - 16u, ctx.r31.u64);
    PPC_STORE_U32(ctx.r1.u32 - 8u, ctx.r12.u32);
}
PPC_FUNC(__restgprlr_28)
{
    ctx.r28.u64 = PPC_LOAD_U64(ctx.r1.u32 - 40u);
    ctx.r29.u64 = PPC_LOAD_U64(ctx.r1.u32 - 32u);
    ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 - 24u);
    ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 - 16u);
    ctx.r12.u64 = PPC_LOAD_U32(ctx.r1.u32 - 8u);
    ctx.lr = ctx.r12.u64;
}
PPC_FUNC(sub_8229F678)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    ResizeArray(memory, *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}
PPC_FUNC(sub_82B7A0B0)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    ctx.r3.u64 = CopyGuestMemory(memory, ctx.r3.u64, ctx.r4.u32,
                                 ctx.r5.u64, ctx.r1.u32);
}
PPC_FUNC(sub_8229C8B0)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    const GuestAddress sp = ctx.r1.u32;
    PPC_STORE_U32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    PPC_STORE_U64(sp - 24u, ctx.r30.u64);
    PPC_STORE_U64(sp - 16u, ctx.r31.u64);
    PPC_STORE_U32(sp - 112u, sp);
    ctx.r3.u64 = string::InitializeString(memory, *active, ctx.r3.u64,
                                          ctx.r4.u64, sp);
    ctx.lr = PPC_LOAD_U32(sp - 8u);
    ctx.r30.u64 = PPC_LOAD_U64(sp - 24u);
    ctx.r31.u64 = PPC_LOAD_U64(sp - 16u);
}
PPC_FUNC(sub_82298938)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    const GuestAddress sp = ctx.r1.u32;
    PPC_STORE_U32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    PPC_STORE_U64(sp - 16u, ctx.r31.u64);
    PPC_STORE_U32(sp - 96u, sp);
    ctx.r3.u64 = ResetTwoByteArray(memory, *active, ctx.r3.u32, sp);
    ctx.lr = PPC_LOAD_U32(sp - 8u);
    ctx.r31.u64 = PPC_LOAD_U64(sp - 16u);
}
PPC_FUNC(sub_822A06C0) { __imp__sub_822A06C0(ctx, base); }
PPC_FUNC(sub_82496948) { __imp__sub_82496948(ctx, base); }
PPC_FUNC(sub_825D5398) { __imp__sub_825D5398(ctx, base); }
PPC_FUNC(sub_825AEEC0) { __imp__sub_825AEEC0(ctx, base); }
PPC_FUNC(sub_825AEB30) { __imp__sub_825AEB30(ctx, base); }
PPC_FUNC(sub_826D6F28) { __imp__sub_826D6F28(ctx, base); }

int main()
{
    try
    {
        for (const Case& test : Cases)
            if (!Compare(test))
                return 1;
        std::array<std::uint8_t, 32> untouched{};
        GuestMemory memory(0, untouched);
        Services services;
        chain::FrameRegisters frame{1, 2, 3, 4, 5};
        std::uint64_t result = 0xdeadbeefcafef00dull;
        if (chain::Apply(0xffffffffu, memory, services, services,
                         0x1234567800012000ull, 0, Stack, frame, result) ||
            result != 0xdeadbeefcafef00dull || frame.lr != 1 ||
            frame.r28 != 2 || frame.r29 != 3 || frame.r30 != 4 ||
            frame.r31 != 5 || untouched != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown property entry changed state");
        std::printf("PASS instance-property-chain 9 exact bodies, %zu bounded comparisons\n",
                    std::size(Cases) + 1);
        std::puts("LIMIT lower ResizeArray/string/copy/reset models reused; "
                  "generic helper ABI and volatile GPR/CR excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
