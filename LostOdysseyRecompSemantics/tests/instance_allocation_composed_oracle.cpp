#include "lo_semantics/instance_allocation_composed_family.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/memory_move.h"

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
namespace family = lo::semantic::gpu::instance_allocation_composed_family;
namespace property = lo::semantic::gpu::string_property_initializer;
namespace string = lo::semantic::gpu::registered_metadata_string;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x12000u, Stack = 0x18000u;
constexpr GuestAddress Manager = 0x8000u, Vtable = 0x8100u;
constexpr GuestAddress ManagerGlobal = 0x8330B608u;
constexpr GuestAddress ResizeMethod = 0x82340000u;
constexpr GuestAddress ReleaseMethod = 0x82340010u;
constexpr GuestAddress FirstMethod = 0x82340020u, SecondMethod = 0x82340030u;
constexpr GuestAddress Headers[] = {0x8336A894u, 0x8336A8D0u,
                                   0x8336A8ACu, 0x8336A8DCu};
enum class Mode { Empty, Existing, Capacity, Negative, Init, CallbackAlias,
                  Holder, HolderAlias, World, Null };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
    {0x823058F0u, __imp__sub_823058F0, Mode::Empty},
    {0x823058F0u, __imp__sub_823058F0, Mode::Existing},
    {0x823058F0u, __imp__sub_823058F0, Mode::Capacity},
    {0x823058F0u, __imp__sub_823058F0, Mode::Negative},
    {0x823058F0u, __imp__sub_823058F0, Mode::Init},
    {0x823058F0u, __imp__sub_823058F0, Mode::CallbackAlias},
    {0x825BA620u, __imp__sub_825BA620, Mode::Holder},
    {0x825BA620u, __imp__sub_825BA620, Mode::HolderAlias},
    {0x825BA858u, __imp__sub_825BA858, Mode::World},
    {0x825BA858u, __imp__sub_825BA858, Mode::Null},
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x50000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve bit-array low window");
        for (GuestAddress page : {0x8330B000u, 0x83318000u, 0x8336A000u})
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit bit-array guest page");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 8> args;
    bool operator==(const Event&) const = default;
};

struct Services final : ArrayResizeServices, ManagerFacadeServices,
                        family::BitWordResizeServices
{
    Mode mode;
    unsigned next_storage = 0;
    std::vector<Event> events;
    explicit Services(Mode value) : mode(value) {}
    void InitializeManager() override
    { throw std::runtime_error("unexpected string manager initialization"); }
    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        events.push_back({'A', {bytes}});
        return 0xAABBCCDD00008000ull;
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    {
        events.push_back({'C', {allocation}});
        return 0x9988776600008000ull;
    }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress method, std::uint64_t receiver) override
    {
        events.push_back({'M', {method, receiver}});
        return 1;
    }
    std::uint64_t ReleaseStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t storage) override
    {
        events.push_back({'L', {method, manager, storage}});
        return storage;
    }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected allocation callback"); }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress storage, std::uint32_t bytes, std::uint32_t alignment) override
    {
        events.push_back({'S', {method, manager, storage, bytes, alignment}});
        return 0x30000u + 0x100u * next_storage++;
    }
    std::uint64_t Resize(GuestAddress method, GuestMemory& memory,
        std::uint64_t manager, std::uint64_t storage, std::uint64_t bytes,
        std::uint64_t alignment, std::uint64_t caller_sp,
        family::FrameRegisters& frame) override
    {
        events.push_back({'R', {method, manager, storage, bytes, alignment,
                               caller_sp, frame.lr, frame.r31}});
        if (method != ResizeMethod || manager != Manager || alignment != 8 ||
            frame.lr != 0x8230595Cu)
            throw std::runtime_error("wrong bit-word resize callback");
        if (mode == Mode::CallbackAlias)
        {
            frame.r31 = 0x9988776600012080ull;
            const GuestAddress saved_r31 = static_cast<GuestAddress>(caller_sp) + 112u;
            memory.WriteU32(saved_r31, 0xAABBCCDDu);
            memory.WriteU32(saved_r31 + 4u, 0x11223344u);
        }
        return 0x1234567800032000ull;
    }
};
Services* active = nullptr;

void Initialize(Window& window, Mode mode)
{
    std::memset(window.bytes, 0xBD, 0x50000);
    for (GuestAddress page : {0x8330B000u, 0x83318000u, 0x8336A000u})
        std::memset(window.bytes + page, 0xBD, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(ManagerGlobal, mode == Mode::Init ? 0 : Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 8u, ResizeMethod | 3u);
    memory.WriteU32(Vtable + 12u, ReleaseMethod | 3u);
    memory.WriteU32(Vtable + 60u, FirstMethod | 3u);
    memory.WriteU32(Vtable + 56u, SecondMethod | 3u);
    memory.WriteU32(0x833180ACu, 0x44556677u);
    memory.WriteU32(Object, mode == Mode::Existing ? 0x26000u : 0);
    memory.WriteU32(Object + 4u, 0);
    memory.WriteU32(Object + 8u, mode == Mode::Empty || mode == Mode::Existing ? 0 :
        mode == Mode::Negative ? 0x7FFFFFFFu : 65u);
    for (unsigned i = 0; i < std::size(Headers); ++i)
    {
        const GuestAddress data = 0x28000u + i * 32u;
        memory.WriteU32(Headers[i], data);
        memory.WriteU32(Headers[i] + 4u, 2);
        memory.WriteU32(Headers[i] + 8u, 2);
        memory.WriteU16(data, static_cast<std::uint16_t>('A' + i));
        memory.WriteU16(data + 2u, 0);
    }
}

bool SameMemory(const Window& a, const Window& b)
{
    if (std::memcmp(a.bytes, b.bytes, 0x50000)) return false;
    for (GuestAddress page : {0x8330B000u, 0x83318000u, 0x8336A000u})
        if (std::memcmp(a.bytes + page, b.bytes + page, 0x1000)) return false;
    return true;
}

bool Compare(const Case& test)
{
    Window original, recovered;
    Initialize(original, test.mode);
    Initialize(recovered, test.mode);
    const GuestAddress object = test.mode == Mode::Null ? 0 :
        test.mode == Mode::HolderAlias ? Stack - 40u : Object;
    PPCContext raw{};
    raw.r1.u64 = 0x5566778800000000ull | Stack;
    raw.r3.u64 = 0x2233445500000000ull | object;
    raw.lr = 0x1122334455667788ull;
    raw.r28.u64 = 0x8877665544332211ull;
    raw.r29.u64 = 0x99AABBCCDDEEFF00ull;
    raw.r30.u64 = 0x1020304050607080ull;
    raw.r31.u64 = 0xA0B0C0D0E0F00112ull;
    const PPCContext initial = raw;
    Services expected(test.mode);
    active = &expected;
    test.original(raw, original.bytes);
    Services actual(test.mode);
    GuestMemory memory(0, std::span<std::uint8_t>(recovered.bytes, Space));
    family::FrameRegisters frame{initial.lr, initial.r28.u64, initial.r29.u64,
                                initial.r30.u64, initial.r31.u64};
    std::uint64_t result = 0xDEADBEEFCAFef00Dull;
    if (!family::Apply(test.address, memory, actual, actual, actual,
        initial.r3.u64, initial.r1.u64, frame, result))
        throw std::runtime_error("unmapped bit-array entry");
    const bool same = raw.r1.u64 == initial.r1.u64 && raw.r3.u64 == result &&
        raw.lr == frame.lr && raw.r28.u64 == frame.r28 && raw.r29.u64 == frame.r29 &&
        raw.r30.u64 == frame.r30 && raw.r31.u64 == frame.r31 &&
        expected.events == actual.events && SameMemory(original, recovered);
    if (!same)
    {
        std::fprintf(stderr, "FAIL bit-array %08x mode %d r3 %016llx/%016llx events %zu/%zu\n",
            test.address, static_cast<int>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result), expected.events.size(), actual.events.size());
        for (GuestAddress offset = 0; offset < 0x50000u; ++offset)
            if (original.bytes[offset] != recovered.bytes[offset])
            {
                std::fprintf(stderr, "first memory difference %08x: %02x/%02x\n",
                    offset, original.bytes[offset], recovered.bytes[offset]);
                break;
            }
        std::fprintf(stderr, "LR %llx/%llx r28 %llx/%llx r29 %llx/%llx r30 %llx/%llx r31 %llx/%llx\n",
            static_cast<unsigned long long>(raw.lr), static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r28.u64), static_cast<unsigned long long>(frame.r28),
            static_cast<unsigned long long>(raw.r29.u64), static_cast<unsigned long long>(frame.r29),
            static_cast<unsigned long long>(raw.r30.u64), static_cast<unsigned long long>(frame.r30),
            static_cast<unsigned long long>(raw.r31.u64), static_cast<unsigned long long>(frame.r31));
    }
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
PPC_FUNC(sub_823058F0) { __imp__sub_823058F0(ctx, base); }
PPC_FUNC(sub_825BA620) { __imp__sub_825BA620(ctx, base); }
PPC_FUNC(sub_82496948) { __imp__sub_82496948(ctx, base); }
PPC_FUNC(sub_822A06C0) { __imp__sub_822A06C0(ctx, base); }
PPC_FUNC(sub_827C5F38) { __imp__sub_827C5F38(ctx, base); }
PPC_FUNC(sub_823ACBD0) { ctx.r3.u64 = active->AllocateRaw(ctx.r3.u32); }
PPC_FUNC(sub_827C5970) { ctx.r3.u64 = active->ConstructPrimary(ctx.r3.u64); }
PPC_FUNC(sub_827C4ED0)
{ ctx.r3.u64 = active->ConstructFallback(ctx.r3.u64, ctx.r4.u32); }

PPC_FUNC(sub_8229F678)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    ResizeArray(memory, *active, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}
PPC_FUNC(sub_82B7A0B0)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    ctx.r3.u64 = CopyGuestMemory(memory, ctx.r3.u64, ctx.r4.u32, ctx.r5.u64, ctx.r1.u32);
}
PPC_FUNC(sub_82B7BC40)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    ctx.r3.u64 = FillGuestMemory(memory, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}
PPC_FUNC(sub_8229C8B0)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    const GuestAddress sp = ctx.r1.u32;
    PPC_STORE_U32(sp - 8u, ctx.lr);
    PPC_STORE_U64(sp - 24u, ctx.r30.u64);
    PPC_STORE_U64(sp - 16u, ctx.r31.u64);
    PPC_STORE_U32(sp - 112u, sp);
    ctx.r3.u64 = string::InitializeString(memory, *active, ctx.r3.u64, ctx.r4.u64, sp);
    ctx.lr = PPC_LOAD_U32(sp - 8u);
    ctx.r30.u64 = PPC_LOAD_U64(sp - 24u);
    ctx.r31.u64 = PPC_LOAD_U64(sp - 16u);
}
PPC_FUNC(sub_82298938)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    const GuestAddress sp = ctx.r1.u32;
    PPC_STORE_U32(sp - 8u, ctx.lr);
    PPC_STORE_U64(sp - 16u, ctx.r31.u64);
    PPC_STORE_U32(sp - 96u, sp);
    ctx.r3.u64 = ResetTwoByteArray(memory, *active, ctx.r3.u32, sp);
    ctx.lr = PPC_LOAD_U32(sp - 8u);
    ctx.r31.u64 = PPC_LOAD_U64(sp - 16u);
}

static void Dispatch(PPCContext& ctx, std::uint8_t* base, std::uint32_t method)
{
    if (method == FirstMethod || method == SecondMethod)
    {
        ctx.r3.u64 = active->CallMethod(method, ctx.r3.u64);
        return;
    }
    if (method != ResizeMethod) throw std::runtime_error("unknown bit-array callback");
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    family::FrameRegisters frame{ctx.lr, ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64};
    ctx.r3.u64 = active->Resize(method, memory, ctx.r3.u64, ctx.r4.u64,
        ctx.r5.u64, ctx.r6.u64, ctx.r1.u64, frame);
    ctx.lr = frame.lr;
    ctx.r28.u64 = frame.r28;
    ctx.r29.u64 = frame.r29;
    ctx.r30.u64 = frame.r30;
    ctx.r31.u64 = frame.r31;
}

int main()
{
    try
    {
        for (const Case& test : Cases) if (!Compare(test)) return 1;
        std::array<std::uint8_t, 32> untouched{};
        GuestMemory memory(0, untouched);
        Services services(Mode::Empty);
        family::FrameRegisters frame{1, 2, 3, 4, 5};
        std::uint64_t result = 99;
        if (family::Apply(0xFFFFFFFFu, memory, services, services, services,
            0, Stack, frame, result) || result != 99 || frame.lr != 1 ||
            frame.r28 != 2 || frame.r29 != 3 || frame.r30 != 4 || frame.r31 != 5 ||
            !services.events.empty() || untouched != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown bit-array entry changed state");
        std::printf("PASS instance-allocation-composed 3 exact bodies, %zu PPC cases + unknown\n",
                    std::size(Cases));
        std::puts("LIMIT existing string/resize/fill/manager models reused; dynamic target internals, "
                  "generic lower ABI and volatile GPR/CR excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
