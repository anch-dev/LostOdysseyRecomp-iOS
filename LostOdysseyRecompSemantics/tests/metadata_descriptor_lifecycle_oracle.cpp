#include "lo_semantics/metadata_descriptor_lifecycle.h"

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
namespace family = lo::semantic::gpu::metadata_descriptor_lifecycle;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr std::uint64_t Stack = 0x1234567800080000ull;
constexpr GuestAddress Object = 0x20000u;
constexpr GuestAddress RedirectedObject = 0x21000u;
constexpr GuestAddress Node1 = 0x30000u;
constexpr GuestAddress Node2 = 0x31000u;
constexpr GuestAddress Node3 = 0x32000u;
constexpr GuestAddress NodeTable = 0x33000u;
constexpr GuestAddress Manager = 0x50000u;
constexpr GuestAddress ManagerTable = 0x50100u;
constexpr GuestAddress Entries52 = 0x60000u;
constexpr GuestAddress Entries32 = 0x61000u;
constexpr GuestAddress Buffer52 = 0x62000u;
constexpr GuestAddress Buffer32 = 0x63000u;
constexpr GuestAddress ReleaseMethod = 0x7403u;
constexpr GuestAddress ResizeMethod = 0x7503u;
constexpr GuestAddress DispatchMethod = 0x7303u;
enum class Mode { Empty, SignedLimit, ZeroAdditional, LiveNode,
    CleanupZero, CleanupArrays, CleanupAlias };
struct Case { GuestAddress address; PPCFunc* original; Mode mode; };
constexpr Case Cases[] = {
    {0x8240efc0u, __imp__sub_8240EFC0, Mode::Empty},
    {0x8240efc0u, __imp__sub_8240EFC0, Mode::SignedLimit},
    {0x8240efc0u, __imp__sub_8240EFC0, Mode::ZeroAdditional},
    {0x8240efc0u, __imp__sub_8240EFC0, Mode::LiveNode},
    {0x82401c58u, __imp__sub_82401C58, Mode::CleanupZero},
    {0x82401c58u, __imp__sub_82401C58, Mode::CleanupArrays},
    {0x82401c58u, __imp__sub_82401C58, Mode::CleanupAlias},
};
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x90000u, MEM_COMMIT,
                PAGE_READWRITE) ||
            !VirtualAlloc(bytes + 0x8330b000u, 0x1000u,
                MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve lifecycle guest RAM");
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 9> args;
    bool operator==(const Event&) const = default;
};
struct Services final : ManagerFacadeServices, family::VirtualServices
{
    GuestMemory memory;
    Mode mode;
    std::vector<Event> events;
    bool alias_done = false;
    Services(std::uint8_t* bytes, Mode selected)
        : memory(0, std::span<std::uint8_t>(bytes, Space)), mode(selected) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected raw manager allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected manager construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager initializer method"); }
    std::uint64_t AllocateStorage(GuestAddress, std::uint64_t,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected manager allocation method"); }
    std::uint64_t ReleaseStorage(GuestAddress method,
        std::uint64_t manager, std::uint64_t buffer) override
    {
        events.push_back({'F', {method, manager, buffer}});
        if (method != (ReleaseMethod & ~3u) || manager != Manager)
            throw std::runtime_error("incorrect manager release boundary");
        if (mode == Mode::CleanupAlias && !alias_done)
        {
            // 823F3340 restores r30/r31 from these slots into 82401C58.
            const GuestAddress sp = static_cast<GuestAddress>(Stack);
            memory.WriteU32(sp - 136u, 0x12345678u);
            memory.WriteU32(sp - 132u, RedirectedObject + 52u);
            memory.WriteU32(sp - 128u, 0xabcdef00u);
            memory.WriteU32(sp - 124u, RedirectedObject);
            alias_done = true;
        }
        return 0xfedcba9876543210ull;
    }
    GuestAddress ResizeStorage(GuestAddress method, GuestAddress manager,
        GuestAddress old, std::uint32_t bytes,
        std::uint32_t argument) override
    {
        events.push_back({'Z', {method, manager, old, bytes, argument}});
        if (method != (ResizeMethod & ~3u) || manager != Manager ||
            bytes > 0x1000u || argument != 8u)
            throw std::runtime_error("incorrect array resize boundary");
        return bytes == 0u ? 0u : old;
    }
    void Dispatch(GuestAddress target, GuestMemory& caller_memory,
        family::VolatileRegisters& registers, std::uint64_t caller_sp,
        family::FrameRegisters& frame) override
    {
        events.push_back({'V', {target, registers.r3, registers.r4,
            registers.r5, registers.r6, registers.r7, caller_sp,
            frame.lr, frame.ctr}});
        const unsigned invocation = static_cast<unsigned>(events.size());
        const GuestAddress expected_node = invocation == 1 ? Node1 : Node2;
        const std::uint64_t expected_r5 =
            mode != Mode::ZeroAdditional && invocation == 1 ?
            0x8877665500000024ull : 0u;
        if (&caller_memory != &memory || target != (DispatchMethod & ~3u) ||
            frame.ctr != DispatchMethod || frame.lr != 0x8240f02cu ||
            caller_sp != Stack - 144u ||
            registers.r3 != expected_node ||
            registers.r5 != expected_r5)
            throw std::runtime_error("incorrect lifecycle virtual boundary");
        if (mode == Mode::LiveNode)
            frame.r31 = 0xabcdef0000032000ull;
        // Deliberately change r5; the next iteration must overwrite it even
        // when the incoming additional amount has low word zero.
        registers.r5 = 0x1111222233334444ull;
        registers.r3 = 0xdeadbeef00000000ull | invocation;
    }
};
Services* active = nullptr;

void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}
void SeedArray(GuestMemory& memory, GuestAddress object,
    GuestAddress offset, GuestAddress entries, GuestAddress buffer,
    bool populated)
{
    const GuestAddress header = object + offset;
    memory.WriteU32(header, populated ? entries : 0u);
    memory.WriteU32(header + 4u, populated ? 1u : 0u);
    memory.WriteU32(header + 8u, populated ? 2u : 0u);
    memory.WriteU32(header + 12u, populated ? buffer : 0u);
    memory.WriteU32(header + 16u, populated ? 0x33u : 0u);
    if (populated)
    {
        memory.WriteU32(entries, 0xabababab);
        memory.WriteU32(entries + 4u, 0xcdcdcdcd);
    }
}
void Seed(Window& window, Mode mode)
{
    std::memset(window.bytes, 0, 0x90000u);
    std::memset(window.bytes + 0x8330b000u, 0, 0x1000u);
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0x8330b608u, Manager);
    memory.WriteU32(Manager, ManagerTable);
    memory.WriteU32(ManagerTable + 12u, ReleaseMethod);
    memory.WriteU32(ManagerTable + 8u, ResizeMethod);
    memory.WriteU32(Node1, NodeTable);
    memory.WriteU32(Node2, NodeTable);
    memory.WriteU32(Node3, NodeTable);
    memory.WriteU32(NodeTable + 360u, DispatchMethod);
    memory.WriteU32(Object + 124u, mode == Mode::Empty ? 0u : Node1);
    memory.WriteU32(Node1 + 100u, 4u);
    memory.WriteU32(Node2 + 100u, 12u);
    memory.WriteU32(Node1 + 124u, Node2);
    memory.WriteU32(Node2 + 124u, 0u);
    memory.WriteU32(Node3 + 124u, 0u);
    const bool populated = mode == Mode::CleanupArrays;
    SeedArray(memory, Object, 52u, Entries52, Buffer52, populated);
    SeedArray(memory, Object, 32u, Entries32, Buffer32, populated);
    SeedArray(memory, RedirectedObject, 52u,
        Entries52 + 0x200u, Buffer52 + 0x200u, false);
    SeedArray(memory, RedirectedObject, 32u,
        Entries32 + 0x200u, Buffer32 + 0x200u, false);
}
PPCContext Initial(const Case& test)
{
    PPCContext raw{};
    raw.r1.u64 = Stack;
    raw.r3.u64 = 0xabcdef0000020000ull;
    raw.r4.u64 = 0x1234567800000010ull;
    raw.r5.u64 = test.mode == Mode::ZeroAdditional ?
        0x8877665500000000ull : 0x8877665500000020ull;
    raw.r6.u64 = 0x1234567800000008ull;
    raw.r7.u64 = 0x1234567800000004ull;
    raw.r8.u64 = 0x1234567800000005ull;
    raw.lr = 0x8877665544332211ull;
    raw.ctr.u64 = 0x1122334455667788ull;
    const std::array<PPCRegister*, 6> saved = {&raw.r26, &raw.r27,
        &raw.r28, &raw.r29, &raw.r30, &raw.r31};
    for (unsigned i = 0; i < saved.size(); ++i)
        saved[i]->u64 = 0xabcdef0000000000ull | (i + 26u);
    return raw;
}
bool Compare(const Case& test)
{
    Window original, recovered;
    Seed(original, test.mode);
    Seed(recovered, test.mode);
    PPCContext raw = Initial(test);
    const PPCContext initial = raw;
    Services expected(original.bytes, test.mode);
    active = &expected;
    test.original(raw, original.bytes);
    Services actual(recovered.bytes, test.mode);
    family::VolatileRegisters registers{initial.r3.u64, initial.r4.u64,
        initial.r5.u64, initial.r6.u64, initial.r7.u64, initial.r8.u64};
    family::FrameRegisters frame{initial.lr, initial.r26.u64,
        initial.r27.u64, initial.r28.u64, initial.r29.u64,
        initial.r30.u64, initial.r31.u64, initial.ctr.u64};
    std::uint64_t result = 0xdeadbeefcafef00dull;
    if (!family::Apply(test.address, actual.memory, actual, actual,
            registers, initial.r1.u64, frame, result))
        throw std::runtime_error("descriptor-lifecycle entry missing");
    std::size_t first = 0;
    while (first < 0x90000u && original.bytes[first] == recovered.bytes[first])
        ++first;
    if (first == 0x90000u)
        for (std::size_t i = 0; i < 0x1000u; ++i)
            if (original.bytes[0x8330b000u + i] !=
                recovered.bytes[0x8330b000u + i])
            { first = 0x8330b000u + i; break; }
    const bool dispatch = test.address == 0x8240efc0u;
    const bool same = (first == 0x90000u) &&
        expected.events == actual.events &&
        expected.alias_done == actual.alias_done &&
        raw.r3.u64 == result && raw.r1.u64 == Stack &&
        raw.lr == frame.lr && raw.r26.u64 == frame.r26 &&
        raw.r27.u64 == frame.r27 && raw.r28.u64 == frame.r28 &&
        raw.r29.u64 == frame.r29 && raw.r30.u64 == frame.r30 &&
        raw.r31.u64 == frame.r31 &&
        (!dispatch || (raw.ctr.u64 == frame.ctr &&
            raw.r4.u64 == registers.r4 && raw.r5.u64 == registers.r5 &&
            raw.r6.u64 == registers.r6 && raw.r7.u64 == registers.r7 &&
            raw.r8.u64 == registers.r8));
    if (!same)
        std::fprintf(stderr,
            "FAIL lifecycle mode=%u r3 %llx/%llx lr %llx/%llx "
            "r31 %llx/%llx events %zu/%zu first %llx:%02x/%02x\n",
            static_cast<unsigned>(test.mode),
            static_cast<unsigned long long>(raw.r3.u64),
            static_cast<unsigned long long>(result),
            static_cast<unsigned long long>(raw.lr),
            static_cast<unsigned long long>(frame.lr),
            static_cast<unsigned long long>(raw.r31.u64),
            static_cast<unsigned long long>(frame.r31),
            expected.events.size(), actual.events.size(),
            static_cast<unsigned long long>(first),
            first < 0x90000u ? original.bytes[first] : 0u,
            first < 0x90000u ? recovered.bytes[first] : 0u);
    return same;
}
} // namespace

void OriginalVirtualCall(PPCContext& ctx, std::uint8_t*, GuestAddress target)
{
    family::VolatileRegisters regs{ctx.r3.u64, ctx.r4.u64, ctx.r5.u64,
        ctx.r6.u64, ctx.r7.u64, ctx.r8.u64};
    family::FrameRegisters frame{ctx.lr, ctx.r26.u64, ctx.r27.u64,
        ctx.r28.u64, ctx.r29.u64, ctx.r30.u64, ctx.r31.u64, ctx.ctr.u64};
    active->Dispatch(target, active->memory, regs, ctx.r1.u64, frame);
    ctx.r3.u64 = regs.r3; ctx.r4.u64 = regs.r4; ctx.r5.u64 = regs.r5;
    ctx.r6.u64 = regs.r6; ctx.r7.u64 = regs.r7; ctx.r8.u64 = regs.r8;
    ctx.lr = frame.lr; ctx.ctr.u64 = frame.ctr;
    ctx.r26.u64 = frame.r26; ctx.r27.u64 = frame.r27;
    ctx.r28.u64 = frame.r28; ctx.r29.u64 = frame.r29;
    ctx.r30.u64 = frame.r30; ctx.r31.u64 = frame.r31;
}
PPC_FUNC(sub_823F3340)
{
    const GuestAddress sp = ctx.r1.u32;
    active->memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    Write64(active->memory, sp - 24u, ctx.r30.u64);
    Write64(active->memory, sp - 16u, ctx.r31.u64);
    active->memory.WriteU32(sp - 112u, sp);
    ctx.r3.u64 = ReleaseManagerBuffer(active->memory, *active,
        ctx.r3.u64, sp);
    ctx.lr = active->memory.ReadU32(sp - 8u);
    ctx.r30.u64 = Read64(active->memory, sp - 24u);
    ctx.r31.u64 = Read64(active->memory, sp - 16u);
}
PPC_FUNC(sub_82507598)
{
    const GuestAddress sp = ctx.r1.u32;
    active->memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(ctx.lr));
    Write64(active->memory, sp - 16u, ctx.r31.u64);
    active->memory.WriteU32(sp - 96u, sp);
    ctx.r3.u64 = ReleaseArrayElements(active->memory, *active,
        ctx.r3.u32, 12u, 8u, sp);
    ctx.lr = active->memory.ReadU32(sp - 8u);
    ctx.r31.u64 = Read64(active->memory, sp - 16u);
}

int main()
{
    try
    {
        for (const Case& test : Cases) if (!Compare(test)) return 1;
        Window spare;
        Services service(spare.bytes, Mode::Empty);
        family::VolatileRegisters regs{1, 2, 3, 4, 5, 6};
        family::FrameRegisters frame{7, 8, 9, 10, 11, 12, 13, 14};
        std::uint64_t result = 15;
        spare.bytes[Object] = 0x5au;
        if (family::Apply(0xffffffffu, service.memory, service, service,
                regs, Stack, frame, result) || result != 15 ||
            regs.r3 != 1 || frame.lr != 7 || frame.r31 != 13 ||
            spare.bytes[Object] != 0x5au || !service.events.empty())
            throw std::runtime_error("unknown lifecycle changed state");
        std::printf("PASS metadata-descriptor-lifecycle %zu original PPC cases + unknown\n",
            std::size(Cases));
        std::puts("LIMIT existing manager/array lower algorithms; deeper generic ABI/frame and virtual internals/MMIO external");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
