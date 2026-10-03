#include "lo_semantics/manager_ring_node.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace
{
using namespace lo::semantic::gpu;
namespace node = lo::semantic::gpu::manager_ring_node;
constexpr std::size_t Space = std::size_t{1} << 32;
constexpr GuestAddress Object = 0x12000u, Stack = 0x18000u;
constexpr GuestAddress Allocation = 0x28000u, RingData = 0x30000u;
constexpr GuestAddress Manager = 0x8000u, Vtable = 0x8100u, Method = 0x7100u;
constexpr GuestAddress Ring = 0x8336A7A4u;
constexpr GuestAddress Pages[] = {0x82000000u, 0x82001000u, 0x82189000u,
    0x82209000u, 0x8330B000u, 0x83318000u, 0x8336A000u};
enum class Mode { Ring, Heap, RingFailure, HeapFailure, EmptyRing, Mutation };
struct Window
{
    std::uint8_t* bytes = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, Space, MEM_RESERVE, PAGE_NOACCESS));
    Window()
    {
        if (!bytes || !VirtualAlloc(bytes, 0x50000, MEM_COMMIT, PAGE_READWRITE))
            throw std::runtime_error("reserve ring node low guest memory");
        std::memset(bytes, 0xBD, 0x50000);
        for (GuestAddress page : Pages)
        {
            if (!VirtualAlloc(bytes + page, 0x1000, MEM_COMMIT, PAGE_READWRITE))
                throw std::runtime_error("commit ring node guest page");
            std::memset(bytes + page, 0xBD, 0x1000);
        }
    }
    ~Window() { if (bytes) VirtualFree(bytes, 0, MEM_RELEASE); }
};
struct Event
{
    char kind;
    std::array<std::uint64_t, 7> args;
    bool operator==(const Event&) const = default;
};
struct Services final : ManagerFacadeServices, ring_reservation::SynchronizationServices,
    instance_manager_link_family::FpServices, node::VirtualServices
{
    Mode mode;
    std::vector<Event> events;
    explicit Services(Mode m) : mode(m) {}
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("unexpected manager initialization"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("unexpected primary constructor"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("unexpected fallback constructor"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("unexpected manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress, std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("unexpected release"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress, GuestAddress,
        std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("unexpected resize"); }
    std::uint64_t AllocateStorage(GuestAddress method, std::uint64_t manager,
        std::uint64_t bytes, std::uint64_t alignment) override
    {
        events.push_back({'A', {method, manager, bytes, alignment}});
        if (mode == Mode::HeapFailure || mode == Mode::RingFailure)
            return 0xAABBCCDD00000000ull;
        return 0xAABBCCDD00028000ull;
    }
    void LightweightSync() override {}
    void DisableFlushMode() override {}
    std::uint64_t Call(GuestAddress method, GuestMemory& memory,
        std::uint64_t receiver, std::uint64_t sp, node::Registers& registers) override
    {
        events.push_back({'V', {method, receiver, sp, registers.lr,
            registers.r28, std::bit_cast<std::uint64_t>(registers.f0),
            std::bit_cast<std::uint64_t>(registers.f13)}});
        if (method != Method || registers.lr != 0x82326678u)
            throw std::runtime_error("wrong ring node dispatch");
        if (mode == Mode::Mutation)
        {
            registers.r28 = 0x6677889900012100ull;
            registers.f13 = 42;
            // The callback can also replace the value later restored by rest28.
            memory.WriteU32(static_cast<GuestAddress>(sp) + 88u, 0x12345678u);
            memory.WriteU32(static_cast<GuestAddress>(sp) + 92u, 0x11223344u);
        }
        return 0xCCDDEEFF12345678ull;
    }
};
Services* active;

void Setup(Window& window, Mode mode)
{
    GuestMemory memory(0, std::span<std::uint8_t>(window.bytes, Space));
    memory.WriteU32(0, Vtable); // Original failure paths dereference guest zero.
    memory.WriteU32(Manager, Vtable);
    memory.WriteU32(Vtable + 4u, Method | 3u);
    memory.WriteU32(0x8330B608u, Manager);
    memory.WriteU32(0x83318040u, mode == Mode::Heap || mode == Mode::HeapFailure ? 0 : 1);
    memory.WriteU32(0x820010C8u, Method | 3u);
    memory.WriteU32(0x822096D4u, Method | 3u);
    memory.WriteU32(0x82000E50u, 0);
    memory.WriteU32(0x8218958Cu, 0x3F800000u);
    const GuestAddress data = mode == Mode::EmptyRing ? 0 : RingData;
    memory.WriteU32(Ring, data);
    memory.WriteU32(Ring + 4u, 0x40000u);
    memory.WriteU32(Ring + 8u, data);
    memory.WriteU32(Ring + 12u, 0);
    memory.WriteU32(Ring + 16u, 0);
    memory.WriteU32(Ring + 20u, 0x28000u);
    memory.WriteU32(Ring + 24u, 16);
}

bool Compare(Mode mode)
{
    Window original, recovered;
    Setup(original, mode); Setup(recovered, mode);
    PPCContext raw{};
    raw.r1.u64 = 0x5566778800000000ull | Stack;
    raw.r3.u64 = 0x2233445500000000ull | Object;
    raw.r4.u64 = 0x99AABBCCDDEEFF00ull;
    raw.lr = 0x1122334455667788ull;
    raw.r28.u64 = 0x8877665544332211ull;
    raw.r29.u64 = 0x99AABBCCDDEEFF00ull;
    raw.r30.u64 = 0x1020304050607080ull;
    raw.r31.u64 = 0xA0B0C0D0E0F00112ull;
    raw.f0.f64 = 12; raw.f13.f64 = 13;
    const PPCContext initial = raw;
    Services expected(mode);
    active = &expected;
    __imp__sub_82326580(raw, original.bytes);
    Services actual(mode);
    GuestMemory memory(0, std::span<std::uint8_t>(recovered.bytes, Space));
    node::Registers registers{initial.lr, initial.r28.u64, initial.r29.u64,
        initial.r30.u64, initial.r31.u64, initial.f0.f64, initial.f13.f64};
    std::uint64_t result = 99;
    if (!node::Apply(0x82326580u, memory, actual, actual, actual, actual,
        initial.r3.u64, initial.r4.u64, initial.r1.u64, registers, result))
        throw std::runtime_error("ring node not mapped");
    bool same = raw.r1.u64 == initial.r1.u64 && raw.r3.u64 == result &&
        raw.lr == registers.lr && raw.r28.u64 == registers.r28 && raw.r29.u64 == registers.r29 &&
        raw.r30.u64 == registers.r30 && raw.r31.u64 == registers.r31 &&
        std::bit_cast<std::uint64_t>(raw.f0.f64) == std::bit_cast<std::uint64_t>(registers.f0) &&
        std::bit_cast<std::uint64_t>(raw.f13.f64) == std::bit_cast<std::uint64_t>(registers.f13) &&
        expected.events == actual.events && !std::memcmp(original.bytes, recovered.bytes, 0x50000);
    for (GuestAddress page : Pages)
        same = same && !std::memcmp(original.bytes + page, recovered.bytes + page, 0x1000);
    if (!same) std::fprintf(stderr, "FAIL ring node mode %d\n", static_cast<int>(mode));
    return same;
}
} // namespace

PPC_FUNC(__savegprlr_28) { __imp____savegprlr_28(ctx, base); }
PPC_FUNC(__restgprlr_28) { __imp____restgprlr_28(ctx, base); }
PPC_FUNC(sub_82290AB8) { __imp__sub_82290AB8(ctx, base); }
PPC_FUNC(sub_82700FD8) { __imp__sub_82700FD8(ctx, base); }
PPC_FUNC(sub_82486C88)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    ctx.r3.u64 = AllocateManagerBuffer(memory, *active, ctx.r3.u64, ctx.r1.u32);
}
static void RingNodeIndirect(PPCContext& ctx, std::uint8_t* base, std::uint32_t method)
{
    GuestMemory memory(0, std::span<std::uint8_t>(base, Space));
    node::Registers registers{ctx.lr, ctx.r28.u64, ctx.r29.u64, ctx.r30.u64,
        ctx.r31.u64, ctx.f0.f64, ctx.f13.f64};
    ctx.r3.u64 = active->Call(method, memory, ctx.r3.u64, ctx.r1.u64, registers);
    ctx.lr = registers.lr;
    ctx.r28.u64 = registers.r28; ctx.r29.u64 = registers.r29;
    ctx.r30.u64 = registers.r30; ctx.r31.u64 = registers.r31;
    ctx.f0.f64 = registers.f0; ctx.f13.f64 = registers.f13;
}
int main()
{
    try
    {
        for (Mode mode : {Mode::Ring, Mode::Heap, Mode::RingFailure,
                          Mode::HeapFailure, Mode::EmptyRing, Mode::Mutation})
            if (!Compare(mode)) return 1;
        std::array<std::uint8_t, 32> bytes{};
        GuestMemory memory(0, bytes);
        Services services(Mode::Heap);
        node::Registers registers{1, 2, 3, 4, 5, 6, 7};
        std::uint64_t result = 99;
        if (node::Apply(0xFFFFFFFFu, memory, services, services, services, services,
            0, 0, Stack, registers, result) || result != 99 || registers.lr != 1 ||
            registers.r28 != 2 || registers.r29 != 3 || registers.r30 != 4 || registers.r31 != 5 ||
            registers.f0 != 6 || registers.f13 != 7 || !services.events.empty() ||
            bytes != std::array<std::uint8_t, 32>{})
            throw std::runtime_error("unknown ring node changed state");
        std::puts("PASS manager-ring-node 1 exact body, 6 original-PPC cases + unknown");
        std::puts("LIMIT prior allocation model, dynamic method internals, generic lower ABI, "
                  "optimized sNaN, hardware synchronization/concurrency and faults/MMIO excluded");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
