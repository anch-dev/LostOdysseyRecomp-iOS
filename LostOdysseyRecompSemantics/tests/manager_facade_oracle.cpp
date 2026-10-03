#include "lo_semantics/manager_facade.h"

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
constexpr GuestAddress Array = 0x10000;
constexpr GuestAddress Data = 0x12000;
constexpr GuestAddress Replacement = 0x21000;
constexpr GuestAddress Manager = 0x35000;
constexpr GuestAddress Vtable = 0x35100;
constexpr GuestAddress Global = 0x8330b608;
constexpr GuestAddress GlobalPage = Global & ~0xfffu;
constexpr GuestAddress ReleaseMethod = 0x82345670;
constexpr GuestAddress AllocateMethod = 0x82345680;
constexpr GuestAddress ResizeMethod = 0x82345690;
constexpr GuestAddress InitFirstMethod = 0x823456a0;
constexpr GuestAddress InitSecondMethod = 0x823456b0;
constexpr std::uint64_t CallbackResult = 0xabcdef0000000077ull;

enum class Kind { Release, Allocate, Clear, ReleaseArray, Reset };
struct Case
{
    Kind kind;
    bool lazy_manager = false;
    bool null_buffer = false;
    bool mutate_callback = false;
    std::uint32_t count = 3;
    std::uint32_t capacity = 3;
    GuestAddress resize_result = Replacement;
};

struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes) throw std::runtime_error("reserve guest window");
        if (!VirtualAlloc(bytes, 0x50000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + GlobalPage, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit guest pages");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};

struct Event
{
    char kind;
    std::array<std::uint64_t, 4> arguments;
    std::array<std::uint32_t, 3> header;
    bool operator==(const Event&) const = default;
};

struct Services : ManagerFacadeServices
{
    GuestMemory memory;
    const Case& test;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, const Case& value)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), test(value) {}

    void Record(char kind, std::array<std::uint64_t, 4> args)
    {
        events.push_back({kind, args, {memory.ReadU32(Array),
            memory.ReadU32(Array + 4), memory.ReadU32(Array + 8)}});
    }

    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        Record('A', {bytes, 0, 0, 0});
        if (bytes != 0x48decu) throw std::runtime_error("unexpected raw bytes");
        return Manager;
    }
    std::uint64_t ConstructPrimary(std::uint64_t allocation) override
    { Record('C', {allocation, 0, 0, 0}); return allocation; }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress method,
        std::uint64_t receiver) override
    {
        Record('M', {method, receiver, 0, 0});
        if (method != InitFirstMethod && method != InitSecondMethod)
            throw std::runtime_error("unexpected init method");
        return 1;
    }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t buffer) override
    {
        Record('F', {method, manager, buffer, 0});
        if (method != ReleaseMethod || manager != Manager)
            throw std::runtime_error("unexpected release method");
        if (test.mutate_callback)
        {
            memory.WriteU32(Array, Replacement);
            memory.WriteU32(Array + 4, 0xaaaaaaaau);
            memory.WriteU32(Array + 8, 0xbbbbbbbbu);
        }
        return CallbackResult;
    }
    std::uint64_t AllocateStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t bytes,
        std::uint64_t alignment) override
    {
        Record('L', {method, manager, bytes, alignment});
        if (method != AllocateMethod || manager != Manager || alignment != 8)
            throw std::runtime_error("unexpected allocate method");
        return CallbackResult;
    }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old_storage, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        Record('R', {method, manager, old_storage,
            (std::uint64_t{bytes} << 32) | argument});
        if (method != ResizeMethod || manager != Manager || argument != 8)
            throw std::runtime_error("unexpected resize method");
        if (test.mutate_callback)
        {
            memory.WriteU32(Array + 4, 2);
            memory.WriteU32(Array + 8, 2);
        }
        return test.resize_result;
    }
};

Services* active = nullptr;

void Initialize(std::uint8_t* bytes, const Case& test)
{
    std::memset(bytes, 0xbd, 0x50000);
    std::memset(bytes + GlobalPage, 0xbd, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(Global, test.lazy_manager ? 0 : Manager);
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 4, AllocateMethod | 3u);
    memory.WriteU32(Vtable + 8, ResizeMethod | 3u);
    memory.WriteU32(Vtable + 12, ReleaseMethod | 3u);
    memory.WriteU32(Vtable + 56, InitSecondMethod | 3u);
    memory.WriteU32(Vtable + 60, InitFirstMethod | 3u);
    memory.WriteU32(Array, test.null_buffer ? 0 : Data);
    memory.WriteU32(Array + 4, test.count);
    memory.WriteU32(Array + 8, test.capacity);
    for (unsigned i = 0; i != 32; ++i)
        memory.WriteU8(Data + i, static_cast<std::uint8_t>(0x40u + i));
}

bool Test(const Case& test, unsigned index, Window& original, Window& recovered)
{
    Initialize(original.bytes, test);
    Initialize(recovered.bytes, test);
    Services expected(original.bytes, test);
    Services actual(recovered.bytes, test);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000;
    context.r3.u64 = test.kind == Kind::Release ?
        0x1234567800012000ull : test.kind == Kind::Allocate ?
        0x12345678000000d0ull : Array;
    active = &expected;
    switch (test.kind)
    {
    case Kind::Release: __imp__sub_823F3340(context, original.bytes); break;
    case Kind::Allocate: __imp__sub_82486C88(context, original.bytes); break;
    case Kind::Clear: __imp__sub_823F3548(context, original.bytes); break;
    case Kind::ReleaseArray: __imp__sub_82298A98(context, original.bytes); break;
    case Kind::Reset: __imp__sub_82298938(context, original.bytes); break;
    }
    std::uint64_t result = 0;
    switch (test.kind)
    {
    case Kind::Release:
        result = ReleaseManagerBuffer(actual.memory, actual,
            0x1234567800012000ull, Stack);
        break;
    case Kind::Allocate:
        result = AllocateManagerBuffer(actual.memory, actual,
            0x12345678000000d0ull, Stack);
        break;
    case Kind::Clear:
        result = ClearBufferHeader(actual.memory, actual, Array, Stack);
        break;
    case Kind::ReleaseArray:
        result = ReleaseTwoByteArray(actual.memory, actual, Array, Stack);
        break;
    case Kind::Reset:
        result = ResetTwoByteArray(actual.memory, actual, Array, Stack);
        break;
    }
    const GuestAddress init_slot = test.kind == Kind::ReleaseArray ?
        Stack - 464u + 80u : Stack - 224u + 80u;
    const bool same = context.r3.u64 == result &&
        context.r1.u64 == Stack && context.lr == 0x82200000 &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0 &&
        std::memcmp(original.bytes + GlobalPage,
                    recovered.bytes + GlobalPage, 0x1000) == 0 &&
        (!test.lazy_manager ||
         std::memcmp(original.bytes + init_slot,
                     recovered.bytes + init_slot, 4) == 0);
    if (!same)
    {
        std::size_t first = 0;
        while (first < Stack - 0x1000u &&
               original.bytes[first] == recovered.bytes[first]) ++first;
        std::fprintf(stderr, "FAIL manager-facade case %u kind %u r3 %llx/%llx events %zu/%zu first %zx\n",
            index, static_cast<unsigned>(test.kind),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(), first);
        return false;
    }
    return true;
}
} // namespace

PPC_FUNC(sub_823F3340) { __imp__sub_823F3340(ctx, base); }
PPC_FUNC(sub_82486C88) { __imp__sub_82486C88(ctx, base); }
PPC_FUNC(sub_823F3548) { __imp__sub_823F3548(ctx, base); }
PPC_FUNC(sub_82298A98) { __imp__sub_82298A98(ctx, base); }
PPC_FUNC(sub_82298938) { __imp__sub_82298938(ctx, base); }
PPC_FUNC(sub_827C5F38)
{
    (void)base;
    ctx.r3.u64 = InitializeManager(active->memory, *active, ctx.r1.u32 - 112u);
}
PPC_FUNC(sub_82298AF8)
{
    (void)base;
    struct ArrayServices : ArrayResizeServices
    {
        Services& services;
        GuestAddress resize_entry_sp;
        ArrayServices(Services& owner, GuestAddress entry)
            : services(owner), resize_entry_sp(entry) {}
        void InitializeManager() override
        { (void)lo::semantic::gpu::InitializeManager(services.memory, services,
            resize_entry_sp - 128u - 112u); }
        GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
            GuestAddress old_storage, std::uint32_t bytes,
            std::uint32_t argument) override
        { return services.ResizeStorage(method, manager, old_storage, bytes, argument); }
    } array_services{*active, ctx.r1.u32 - 128u};
    RemoveArrayRange(active->memory, array_services, ctx.r3.u32, ctx.r4.u32,
        ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r1.u32 - 128u);
}
PPC_FUNC(sub_8229F678)
{
    (void)base;
    struct ArrayServices : ArrayResizeServices
    {
        Services& services;
        GuestAddress entry_sp;
        ArrayServices(Services& owner, GuestAddress entry)
            : services(owner), entry_sp(entry) {}
        void InitializeManager() override
        { (void)lo::semantic::gpu::InitializeManager(services.memory, services,
            entry_sp - 128u - 112u); }
        GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
            GuestAddress old_storage, std::uint32_t bytes,
            std::uint32_t argument) override
        { return services.ResizeStorage(method, manager, old_storage, bytes, argument); }
    } array_services{*active, ctx.r1.u32};
    ResizeArray(active->memory, array_services, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
}
void ManagerFacadeIndirect(PPCContext& ctx, std::uint8_t*, std::uint32_t method)
{
    if (method == ReleaseMethod)
        ctx.r3.u64 = active->ReleaseStorage(method, ctx.r3.u64, ctx.r4.u64);
    else if (method == AllocateMethod)
        ctx.r3.u64 = active->AllocateStorage(method, ctx.r3.u64,
                                               ctx.r4.u64, ctx.r5.u64);
    else
        throw std::runtime_error("unexpected manager indirect method");
}

int main()
{
    try
    {
        const Case cases[] = {
            {Kind::Release}, {Kind::Release, true},
            {Kind::Allocate}, {Kind::Allocate, true},
            {Kind::Clear, false, true},
            {Kind::Clear, false, false, true},
            {Kind::ReleaseArray, false, false, true},
            {Kind::ReleaseArray, true, false, true},
            {Kind::ReleaseArray, false, true, false, 0, 0},
            {Kind::Reset, false, false, true, 3, 0},
            {Kind::Reset, false, false, true, 3, 3},
            {Kind::Reset, false, false, false, 3, 3, 0},
        };
        Window original, recovered;
        for (unsigned index = 0; index != std::size(cases); ++index)
            if (!Test(cases[index], index, original, recovered)) return 1;
        std::puts("PASS manager-facade 12 composed original-PPC cases");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
