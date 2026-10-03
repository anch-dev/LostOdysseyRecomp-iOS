#include "lo_semantics/object_ring_dispatch.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_ring_dispatch;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr test::Region Regions[] = {
    {0, 0x100000u}, {0x83318000u, 0x1000u}, {0x8336a000u, 0x2000u}
};
constexpr GuestAddress Object = 0x60000u, Owner = 0x62000u, Old = 0x63000u;
constexpr GuestAddress Ring = 0x8336a7a4u, Triplet = 0x8336a9b4u;
constexpr std::uint64_t InitialSp = 0x1234567800080000ull;

enum class Mode { OldZero, OldDifferent, OldEqualsOwner, RingObject, RingZero };
struct Case { GuestAddress entry; Mode mode; };
constexpr Case Cases[] = {
    {0x823efa30u, Mode::OldZero},
    {0x823efa30u, Mode::OldDifferent},
    {0x823efa30u, Mode::OldEqualsOwner},
    {0x82372fb0u, Mode::OldZero},
    {0x82372fb0u, Mode::OldDifferent},
    {0x82372fb0u, Mode::RingObject},
    {0x82372fb0u, Mode::RingZero}
};

struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};

using Event = std::array<std::uint64_t, 7>;
struct Services final : ring_reservation::SynchronizationServices,
    float_triplet_transfer::NativeServices, family::DynamicServices
{
    GuestMemory memory;
    std::vector<Event> events;
    unsigned sync_calls = 0, fp_calls = 0;
    explicit Services(test::GuestWindow& window) : memory(window.Memory()) {}
    void LightweightSync() override { ++sync_calls; }
    void SetHostFpControl(std::uint32_t control) override
    { ++fp_calls; PPCFPSCRRegister{}.setcsr(control); }
    void Call(GuestAddress target, GuestMemory& guest, family::Registers& state) override
    {
        events.push_back({target, state.r[3], state.r[4], state.sp,
            state.lr, state.ctr, state.r[31]});
        if (target != 0x9000u && target != 0x9100u && target != 0x9200u)
            throw std::runtime_error("unexpected dynamic target");
        guest.WriteU32(Object + 516u, 0xabc00000u + target);
        state.r[3] = 0xface000000000000ull | target;
        state.r[5] = 0xbeef000000000000ull | target;
        state.r[10] = 0xcafe000000000000ull | target;
        state.r[11] = 0xdade000000000000ull | target;
        state.ctr = 0xabba000000000000ull | target;
        state.lr = 0xfeee000000000000ull | target;
        state.cr0 = {1, 0, 0, 1};
        state.cr6 = {0, 1, 0, 1};
        state.xer_so = 1;
        state.xer_ca = 1;
        state.f0_bits = 0x4009000000000000ull;
        if (target == 0x9100u)
            guest.WriteU32(Object + 340u, 0x40400000u);
    }
};
Services* active = nullptr;

family::Registers FromPpc(const PPCContext& context)
{
    family::Registers state{};
    const PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
        state.r[index] = index == 1u ? 0u : fields[index]->u64;
    state.sp = context.r1.u64;
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    state.cr0 = {context.cr0.lt, context.cr0.gt, context.cr0.eq, context.cr0.so};
    state.cr6 = {context.cr6.lt, context.cr6.gt, context.cr6.eq, context.cr6.so};
    state.f0_bits = context.f0.u64;
    state.cached_fp_control = context.fpscr.csr;
    return state;
}

void ToPpc(PPCContext& context, const family::Registers& state)
{
    PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
        if (index != 1u) fields[index]->u64 = state.r[index];
    context.r1.u64 = state.sp;
    context.lr = state.lr;
    context.ctr.u64 = state.ctr;
    context.xer.so = state.xer_so;
    context.xer.ca = state.xer_ca;
    context.cr0 = {bool(state.cr0.lt), bool(state.cr0.gt),
        bool(state.cr0.eq), {bool(state.cr0.so)}};
    context.cr6 = {bool(state.cr6.lt), bool(state.cr6.gt),
        bool(state.cr6.eq), {bool(state.cr6.so)}};
    context.f0.u64 = state.f0_bits;
    context.fpscr.csr = state.cached_fp_control;
}

bool Same(const family::Registers& left, const family::Registers& right)
{
    if (left.sp != right.sp || left.lr != right.lr || left.ctr != right.ctr ||
        left.xer_so != right.xer_so || left.xer_ca != right.xer_ca ||
        left.f0_bits != right.f0_bits ||
        left.cached_fp_control != right.cached_fp_control ||
        left.cr0.lt != right.cr0.lt || left.cr0.gt != right.cr0.gt ||
        left.cr0.eq != right.cr0.eq || left.cr0.so != right.cr0.so ||
        left.cr6.lt != right.cr6.lt || left.cr6.gt != right.cr6.gt ||
        left.cr6.eq != right.cr6.eq || left.cr6.so != right.cr6.so)
        return false;
    for (unsigned index = 0; index < 32u; ++index)
        if (index != 1u && left.r[index] != right.r[index]) return false;
    return true;
}

void Seed(test::GuestWindow& window, const Case& item)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(0x83318040u,
        item.mode == Mode::RingObject || item.mode == Mode::RingZero ? 1u : 0u);
    memory.WriteU32(Object, 0x70000u);
    memory.WriteU32(0x70000u + 84u, 0x9003u);
    memory.WriteU32(0x70000u + 56u, 0x9101u);
    memory.WriteU32(Old, 0x71000u);
    memory.WriteU32(0x71000u, 0x9202u);
    const GuestAddress previous = item.mode == Mode::OldDifferent ? Old :
        item.mode == Mode::OldEqualsOwner ? Owner : 0u;
    memory.WriteU32(Object + 504u, previous);
    memory.WriteU32(Object + 336u, 0x3fc00000u);
    memory.WriteU32(Object + 340u, 0xc0100000u);
    memory.WriteU32(Object + 344u, 0x3f000000u);
    memory.WriteU32(Ring, 0x64000u);
    memory.WriteU32(Ring + 4u, 0x65000u);
    const auto cursor = item.mode == Mode::RingZero ? 0u : 0x64020u;
    memory.WriteU32(Ring + 8u, cursor);
    memory.WriteU32(Ring + 20u, cursor);
    memory.WriteU32(Ring + 24u, 16u);
    memory.WriteU32(Triplet, 0x11111111u);
    memory.WriteU32(Triplet + 4u, 0x22222222u);
    memory.WriteU32(Triplet + 8u, 0x33333333u);
}

void Check(const Case& item)
{
    RestoreHost host;
    test::GuestWindow original(Regions), recovered(Regions);
    Seed(original, item); Seed(recovered, item);
    Services expected(original), actual(recovered);
    PPCContext context{};
    context.r1.u64 = InitialSp;
    context.lr = 0x99887766abcdef01ull;
    context.r3.u64 = 0x1122334400000000ull | Object;
    context.r4.u64 = 0x8877665500000000ull | Owner;
    context.r5.u64 = 0x1111000022220000ull;
    context.r7.u64 = 7;
    context.r8.u64 = 8;
    context.r9.u64 = 9;
    context.r10.u64 = 10;
    context.r11.u64 = 11;
    context.r12.u64 = 0x1122334455667788ull;
    context.r28.u64 = 0x1011121314151617ull;
    context.r29.u64 = 0x2021222324252627ull;
    context.r30.u64 = 0x3031323334353637ull;
    context.r31.u64 = 0x4041424344454647ull;
    context.xer.so = 0;
    context.xer.ca = 0;
    context.f0.u64 = 0x3ff0000000000000ull;
    context.fpscr.csr = item.mode == Mode::RingObject || item.mode == Mode::RingZero
        ? 0x1f80u : 0x9fc0u;
    auto state = FromPpc(context);

    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    switch (item.entry)
    {
    case 0x823efa30u: __imp__sub_823EFA30(context, original.Bytes()); break;
    case 0x82372fb0u: __imp__sub_82372FB0(context, original.Bytes()); break;
    }
    active = nullptr;
    const auto original_host_control = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    const family::Dependencies dependencies{actual, actual, actual};
    if (!family::Apply(item.entry, actual.memory, dependencies, state))
        throw std::runtime_error("unmapped object ring dispatch entry");
    const auto recovered_host_control = PPCFPSCRRegister{}.getcsr();

    const bool ring = item.mode == Mode::RingObject || item.mode == Mode::RingZero;
    const auto expected_calls = ring ? 0u :
        item.mode == Mode::OldDifferent ? 3u : 2u;
    if (expected.events.size() != expected_calls ||
        (expected_calls && (expected.events.front()[0] != 0x9000u ||
            expected.events.back()[0] != 0x9100u)) ||
        (expected_calls == 3u && expected.events[1][0] != 0x9200u) ||
        expected.events != actual.events ||
        actual.sync_calls != unsigned(item.mode == Mode::RingObject) ||
        !Same(FromPpc(context), state) ||
        original_host_control != recovered_host_control ||
        !original.EqualCommitted(recovered))
    {
        const auto observed = FromPpc(context);
        std::fprintf(stderr, "entry=%X mode=%u state=%u RAM=%u sync=%u host=%X/%X events=%zu/%zu\n",
            item.entry, unsigned(item.mode), Same(observed, state),
            original.EqualCommitted(recovered), actual.sync_calls,
            original_host_control, recovered_host_control,
            expected.events.size(), actual.events.size());
        for (unsigned index = 0; index < 32u; ++index)
            if (observed.r[index] != state.r[index])
                std::fprintf(stderr, "r%u=%llX/%llX\n", index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        throw std::runtime_error("object ring dispatch original PPC comparison");
    }

    const auto memory = original.Memory();
    if (ring)
    {
        if (memory.ReadU32(Triplet) != 0x11111111u ||
            (item.mode == Mode::RingObject &&
                (memory.ReadU32(0x64020u) != 0x82003654u ||
                 memory.ReadU32(Ring + 8u) != 0x64030u)) ||
            (item.mode == Mode::RingZero && memory.ReadU32(Ring + 8u) != 0u))
            throw std::runtime_error("object ring fixture missed reservation path");
    }
    else if (memory.ReadU32(Triplet) != 0x3fc00000u ||
        memory.ReadU32(Triplet + 4u) != 0x40400000u ||
        memory.ReadU32(Triplet + 8u) != 0x3f000000u)
        throw std::runtime_error("object ring fixture missed live FP path");
}
}

void OriginalSave(PPCContext& context, std::uint8_t*)
{
    auto& memory = active->memory;
    WriteU64(memory, Address(context.r1.u64 - 40u), context.r28.u64);
    WriteU64(memory, Address(context.r1.u64 - 32u), context.r29.u64);
    WriteU64(memory, Address(context.r1.u64 - 24u), context.r30.u64);
    WriteU64(memory, Address(context.r1.u64 - 16u), context.r31.u64);
    memory.WriteU32(Address(context.r1.u64 - 8u), Address(context.r12.u64));
}

void OriginalRestore(PPCContext& context, std::uint8_t*)
{
    auto& memory = active->memory;
    context.r28.u64 = ReadU64(memory, Address(context.r1.u64 - 40u));
    context.r29.u64 = ReadU64(memory, Address(context.r1.u64 - 32u));
    context.r30.u64 = ReadU64(memory, Address(context.r1.u64 - 24u));
    context.r31.u64 = ReadU64(memory, Address(context.r1.u64 - 16u));
    context.r12.u64 = memory.ReadU32(Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}

void OriginalIndirect(GuestAddress target, PPCContext& context, std::uint8_t*)
{
    auto state = FromPpc(context);
    active->Call(target, active->memory, state);
    ToPpc(context, state);
}

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS object-ring-dispatch %zu focused original PPC cases\n", std::size(Cases));
        std::puts("LIMIT accepted ring lower; dynamic dispatch callbacks and FP host control bounded, generated lwsync omits hardware ordering; faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
