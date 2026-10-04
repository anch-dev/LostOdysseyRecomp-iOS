// Appended after the complete 82479058 PPC body.
#include "lo_semantics/legacy_config_dispatch_gate.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_config_dispatch_gate;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Object = 0x30000u;
constexpr GuestAddress Vtable = 0x40000u;
constexpr GuestAddress FlagRoot = 0x61000u;
constexpr GuestAddress FlagRecord = 0x62000u;
constexpr GuestAddress Sink = 0x70000u;
constexpr GuestAddress GateGlobal = 0x83246260u;
constexpr GuestAddress RootGlobal = 0x833690d0u;
constexpr GuestAddress Target = 0x91000010u;
constexpr std::array<Region, 3> Regions{{{0u, 0x90000u},
    {0x83246000u, 0x1000u}, {0x83369000u, 0x1000u}}};

enum class Mode { GateOff, GateOpen, GateClosed, TaggedTarget };
constexpr std::array Cases{Mode::GateOff, Mode::GateOpen,
    Mode::GateClosed, Mode::TaggedTarget};

PPCRegister* Gpr(PPCContext& context, unsigned index)
{
    PPCRegister* const fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16,
        &context.r17, &context.r18, &context.r19, &context.r20,
        &context.r21, &context.r22, &context.r23, &context.r24,
        &context.r25, &context.r26, &context.r27, &context.r28,
        &context.r29, &context.r30, &context.r31};
    return fields[index];
}

family::Registers FromPpc(PPCContext& context)
{
    family::Registers state{};
    for (unsigned i = 0; i < 32; ++i) state.r[i] = Gpr(context, i)->u64;
    state.sp = context.r1.u64;
    state.lr = context.lr;
    state.ctr = context.ctr.u64;
    state.xer_so = context.xer.so;
    state.xer_ca = context.xer.ca;
    state.cr0 = {std::uint8_t(context.cr0.lt),
        std::uint8_t(context.cr0.gt), std::uint8_t(context.cr0.eq),
        std::uint8_t(context.cr0.so)};
    state.cr6 = {std::uint8_t(context.cr6.lt),
        std::uint8_t(context.cr6.gt), std::uint8_t(context.cr6.eq),
        std::uint8_t(context.cr6.so)};
    return state;
}

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Object, Vtable);
    memory.WriteU32(Object + 8u, 0x4567u);
    memory.WriteU32(Vtable + 4u,
        mode == Mode::TaggedTarget ? Target | 3u : Target);
    memory.WriteU32(GateGlobal, mode == Mode::GateOff ? 0u : 1u);
    memory.WriteU32(RootGlobal, FlagRoot);
    memory.WriteU32(FlagRoot + 3040u, FlagRecord);
    recovery_abi::WriteU64(memory, FlagRecord + 4u,
        mode == Mode::GateClosed ? std::uint64_t{1} << 44 : 0u);
}

PPCContext Initial()
{
    PPCContext context{};
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = 0x1122334400000000ull + i;
    context.r1.u64 = 0x8877665500080000ull;
    context.r3.u64 = 0x9988776600000000ull | Object;
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.lt = 1;
    context.cr6.gt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

void VirtualAction(GuestAddress target, GuestMemory& memory,
    family::Registers& state)
{
    memory.WriteU32(Sink, target);
    memory.WriteU32(Sink + 4u, static_cast<GuestAddress>(state.r[5]));
    state.r[3] = memory.ReadU32(Object + 8u);
}

struct Services final : family::VirtualCalls
{
    unsigned calls = 0;
    void Call(GuestAddress target, GuestMemory& memory,
        family::Registers& state) override
    { ++calls; VirtualAction(target, memory, state); }
};

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so &&
        a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

unsigned OriginalCalls = 0;

void Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto context = Initial();
    auto state = FromPpc(context);
    OriginalCalls = 0;
    __imp__sub_82479058(context, original.Bytes());
    const bool blocked = mode == Mode::GateClosed;
    const auto expected_result = blocked ?
        (0x9988776600000000ull | Object) : 0x4567ull;
    if (context.r3.u64 != expected_result ||
        OriginalCalls != (blocked ? 0u : 1u) ||
        (!blocked &&
            (original.Memory().ReadU32(Sink) != Target ||
                original.Memory().ReadU32(Sink + 4u) != 760u)))
    {
        std::fprintf(stderr, "EXPECT gate case=%u r3=%llX/%llX calls=%u\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(context.r3.u64),
            static_cast<unsigned long long>(expected_result), OriginalCalls);
        throw std::runtime_error("independent config gate outcome");
    }
    Services services;
    auto memory = recovered.Memory();
    if (!family::Apply(0x82479058u, memory, services, state))
        throw std::runtime_error("config gate entry missing");
    const auto observed = FromPpc(context);
    const bool state_equal = Same(observed, state);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!state_equal || !ram_equal || OriginalCalls != services.calls)
    {
        std::fprintf(stderr, "FAIL gate case=%u state=%u RAM=%u "
            "calls=%u/%u r3=%llX/%llX r11=%llX/%llX "
            "CR6=%u%u%u%u/%u%u%u%u\n",
            static_cast<unsigned>(mode), state_equal, ram_equal,
            OriginalCalls, services.calls,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[11]),
            static_cast<unsigned long long>(state.r[11]),
            observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
            observed.cr6.so, state.cr6.lt, state.cr6.gt, state.cr6.eq,
            state.cr6.so);
        for (unsigned i = 0; i < 32; ++i)
            if (observed.r[i] != state.r[i])
                std::fprintf(stderr, " r%u=%llX/%llX", i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        std::fputc('\n', stderr);
        throw std::runtime_error("config gate PPC mismatch");
    }
}
} // namespace

void OriginalVirtualGate(PPCContext& context, std::uint8_t* base)
{
    auto state = FromPpc(context);
    auto memory = GuestMemory(0u,
        std::span<std::uint8_t>(base, GuestWindow::Space));
    VirtualAction(context.ctr.u32 & ~3u, memory, state);
    ++OriginalCalls;
    for (unsigned i = 0; i < 32; ++i)
        Gpr(context, i)->u64 = state.r[i];
}

int main()
{
    try
    {
        for (const auto mode : Cases) Check(mode);
        std::printf("PASS legacy-config-dispatch-gate %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT mutable vtable target; faults/MMIO/concurrency/runtime");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
