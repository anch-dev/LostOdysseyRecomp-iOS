#include "lo_semantics/registered_inline_constructor.h"
#include "lo_semantics/manager_init.h"
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
constexpr GuestAddress Stack = 0x5f000u;
constexpr GuestAddress Frame = Stack - 160u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Manager = 0x30000u;
constexpr GuestAddress ManagerVtable = 0x30100u;
constexpr GuestAddress ManagerGlobal = 0x8330b608u;
constexpr GuestAddress AllocateMethod = 0x82345680u;
constexpr GuestAddress FirstInitMethod = 0x823456c0u;
constexpr GuestAddress SecondInitMethod = 0x823456e0u;
constexpr std::uint64_t Owner = 0xabcdef0000023456ull;
constexpr std::uint64_t FullObject = 0x1234567800010000ull;
constexpr std::uint64_t FailedObject = 0x1234567800000000ull;

enum class Mode { Success, LowWordZero, ColdManager, StackAlias };
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x70000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000, MEM_COMMIT, PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x83315000u, 0x1000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("commit inline constructor guest window");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 4> args;
    bool operator==(const Event&) const = default;
};
struct Services final : ManagerFacadeServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t bytes) override
    {
        events.push_back({'N', {bytes, 0, 0, 0}});
        if (bytes != 0x48decu) throw std::runtime_error("manager allocation size");
        return Manager;
    }
    std::uint64_t ConstructPrimary(std::uint64_t object) override
    {
        events.push_back({'P', {object, 0, 0, 0}});
        return object;
    }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress method, std::uint64_t receiver) override
    {
        events.push_back({'M', {method, receiver, 0, 0}});
        if (receiver != Manager ||
            (method != FirstInitMethod && method != SecondInitMethod))
            throw std::runtime_error("unexpected manager initialization method");
        return 1;
    }
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
        if (method != AllocateMethod || manager != Manager || bytes != 376 ||
            alignment != 8)
            throw std::runtime_error("unexpected allocation arguments");
        if (mode == Mode::LowWordZero) return FailedObject;
        if (mode == Mode::StackAlias)
            return 0x1234567800000000ull | (Frame + 80u);
        return FullObject;
    }
};
Services* active = nullptr;
std::vector<std::array<std::uint64_t, 4>> saved;
void Initialize(std::uint8_t* bytes, Mode mode)
{
    std::memset(bytes, 0xbd, 0x70000);
    std::memset(bytes + 0x8330b000u, 0, 0x1000);
    std::memset(bytes + 0x83315000u, 0, 0x1000);
    GuestMemory memory(0, std::span<std::uint8_t>(bytes, Space));
    memory.WriteU32(ManagerGlobal, mode == Mode::ColdManager ? 0u : Manager);
    memory.WriteU32(Manager, ManagerVtable);
    memory.WriteU32(ManagerVtable + 4, AllocateMethod | 3u);
    memory.WriteU32(ManagerVtable + 56, SecondInitMethod | 3u);
    memory.WriteU32(ManagerVtable + 60, FirstInitMethod | 3u);
    memory.WriteU32(0x83315ed8u, 0);
    memory.WriteU32(0x83315ef0u, 0x24000u);
}
bool CompareOne(Mode mode, Window& original, Window& recovered)
{
    Initialize(original.bytes, mode);
    Initialize(recovered.bytes, mode);
    Services expected(original.bytes, mode), actual(recovered.bytes, mode);
    PPCContext context{};
    context.r1.u64 = Stack;
    context.lr = 0x82200000u;
    context.r3.u64 = Owner;
    context.r29.u64 = 0x1122334455667788ull;
    context.r30.u64 = 0x2233445566778899ull;
    context.r31.u64 = 0x33445566778899aaull;
    saved.clear();
    active = &expected;
    __imp__sub_827CE240(context, original.bytes);
    active = &actual;
    const auto result = ConstructInlineManagedRegisteredObject(
        actual.memory, actual, Owner, Stack);
    bool same = context.r3.u64 == result && context.r1.u64 == Stack &&
        context.lr == 0x82200000u && saved.empty() &&
        context.r29.u64 == 0x1122334455667788ull &&
        context.r30.u64 == 0x2233445566778899ull &&
        context.r31.u64 == 0x33445566778899aaull &&
        expected.events == actual.events &&
        std::memcmp(original.bytes, recovered.bytes, Stack - 0x1000u) == 0 &&
        std::memcmp(original.bytes + 0x8330b000u,
            recovered.bytes + 0x8330b000u, 0x1000) == 0 &&
        std::memcmp(original.bytes + 0x83315000u,
            recovered.bytes + 0x83315000u, 0x1000) == 0;
    for (GuestAddress offset : {80u, 84u, 92u, 100u, 108u, 112u})
        same &= expected.memory.ReadU32(Frame + offset) ==
                actual.memory.ReadU32(Frame + offset);
    if (mode == Mode::StackAlias)
        same &= std::memcmp(original.bytes + Frame + 80u,
            recovered.bytes + Frame + 80u, 376) == 0;
    if (!same)
        std::fprintf(stderr, "FAIL inline constructor mode %u r3 %llx/%llx events %zu/%zu scratch %08x/%08x\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(result),
            expected.events.size(), actual.events.size(),
            expected.memory.ReadU32(Frame + 112u),
            actual.memory.ReadU32(Frame + 112u));
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_29)
{
    (void)base;
    saved.push_back({ctx.r29.u64, ctx.r30.u64, ctx.r31.u64, ctx.r12.u64});
}
PPC_FUNC(__restgprlr_29)
{
    (void)base;
    if (saved.empty()) throw std::runtime_error("unmatched GPR restore");
    const auto registers = saved.back();
    saved.pop_back();
    ctx.r29.u64 = registers[0];
    ctx.r30.u64 = registers[1];
    ctx.r31.u64 = registers[2];
    ctx.lr = registers[3];
}
PPC_FUNC(sub_827C5F38)
{
    (void)base;
    ctx.r3.u64 = InitializeManager(active->memory, *active,
        ctx.r1.u32 - 112u);
}
PPC_FUNC(sub_82410A28)
{
    (void)base;
    ctx.r3.u64 = InitializeRegisteredObject(active->memory,
        RegisteredObjectInput{ctx.r3.u64, ctx.r5.u64, ctx.r6.u64,
            ctx.r7.u64, ctx.r8.u64, ctx.r9.u64, ctx.r10.u64, ctx.r1.u32});
}
void OriginalAllocate(PPCContext& ctx, std::uint8_t* base,
    std::uint32_t method)
{
    (void)base;
    ctx.r3.u64 = active->AllocateStorage(method, ctx.r3.u64,
        ctx.r4.u64, ctx.r5.u64);
}
int main()
{
    try
    {
        Window original, recovered;
        for (Mode mode : {Mode::Success, Mode::LowWordZero,
                          Mode::ColdManager, Mode::StackAlias})
            if (!CompareOne(mode, original, recovered)) return 1;
        std::puts("PASS registered-inline constructor 827CE240 4 cases");
        std::puts("LIMIT exact cached PPC body; manager initializer and registered-object initializer reused; guest vtable allocation is an explicit service boundary");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
