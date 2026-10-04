// Appended after the four exact original PPC bodies in the validation pin.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_reallocation_context.h"
#include "lo_semantics/raw_allocation_context.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace realloc_family = crt_reallocation_context;
using ReallocFull = realloc_family::Registers;
using ReallocHeap = raw_allocation_context::Registers;
constexpr GuestAddress ReallocHeapAddress = 0x10000u;
constexpr GuestAddress ReallocOldPointer = 0x30010u;
constexpr GuestAddress ReallocHandlerTarget = 0x2a00u;
constexpr std::uint64_t ReallocNewPointer = 0x7766554400030020ull;
constexpr GuestAddress ReallocTable = 0x832150a8u;
constexpr std::array<test::Region, 6> ReallocRegions{{
    {0u, 0x120000u}, {0x831e0000u, 0x10000u},
    {0x83214000u, 0x3000u}, {0x83245000u, 0x1000u},
    {0x832d3000u, 0x2000u}, {0x83378000u, 0x3000u}}};

enum class ReallocScenario
{NullAllocate, FreeZero, Success, StatusFallback, HandlerRetry, Oversize};
constexpr std::array ReallocScenarios{ReallocScenario::NullAllocate,
    ReallocScenario::FreeZero, ReallocScenario::Success,
    ReallocScenario::StatusFallback, ReallocScenario::HandlerRetry,
    ReallocScenario::Oversize};
using ReallocEvent = std::array<std::uint64_t, 10>;

ReallocEvent ReallocRecord(GuestAddress entry, const ReallocFull& state)
{
    return {entry, state.r[1], state.lr, state.r[3], state.r[4],
        state.r[5], state.r[6], state.fpr_bits[7],
        state.cr1.lt | (std::uint64_t(state.cr1.gt) << 8u) |
            (std::uint64_t(state.cr1.eq) << 16u) |
            (std::uint64_t(state.cr1.so) << 24u),
        state.cr7.lt | (std::uint64_t(state.cr7.gt) << 8u) |
            (std::uint64_t(state.cr7.eq) << 16u) |
            (std::uint64_t(state.cr7.so) << 24u)};
}

void ReallocMutateVolatile(ReallocFull& state)
{
    state.r[8] ^= 0x123456789abcdef0ull;
    state.fpr_bits[7] ^= 0x0000000000000180ull;
    state.cached_fp_control ^= 0x40u;
    state.cr1 = {0, 1, 0, state.xer_so};
    state.cr7 = {1, 0, 0, state.xer_so};
    state.xer_ca ^= 1u;
}

struct ReallocHeapBoundary final : heap_allocation_context::BoundaryServices
{
    void CallDirect(GuestAddress, GuestMemory&, ReallocHeap&) override
    { throw std::runtime_error("unexpected CRT realloc heap guest"); }
    void CallNative(GuestAddress, GuestMemory&, ReallocHeap&) override
    { throw std::runtime_error("unexpected CRT realloc heap native"); }
};

struct ReallocGuest final : realloc_family::GuestServices
{
    ReallocScenario scenario;
    std::vector<ReallocEvent> events;
    explicit ReallocGuest(ReallocScenario selected) : scenario(selected) {}
    void CallLower(GuestAddress entry, GuestMemory& memory,
        ReallocFull& state) override
    {
        events.push_back(ReallocRecord(entry, state));
        memory.WriteU32(0x54000u +
            static_cast<GuestAddress>((events.size() - 1u) * 4u), entry);
        ReallocMutateVolatile(state);
        if (scenario == ReallocScenario::NullAllocate &&
            entry == 0x823acbd0u && events.size() == 1u)
        { state.r[3] = ReallocNewPointer; return; }
        if (scenario == ReallocScenario::FreeZero &&
            entry == 0x823addc0u && events.size() == 1u)
        { state.r[3] = 0xdeadbeef00000001ull; return; }
        if (entry == 0x827ccf80u)
        {
            if (scenario == ReallocScenario::Success &&
                events.size() == 1u)
            { state.r[3] = ReallocNewPointer; return; }
            if (scenario == ReallocScenario::StatusFallback &&
                events.size() == 1u)
            { state.r[3] = 0u; return; }
            if (scenario == ReallocScenario::HandlerRetry &&
                events.size() <= 2u)
            {
                state.r[3] = events.size() == 1u ? 0u : ReallocNewPointer;
                return;
            }
        }
        throw std::runtime_error("unexpected CRT realloc guest sequence");
    }
};

struct ReallocHandler final : crt_record_allocation_context::HandlerServices
{
    ReallocScenario scenario;
    std::vector<ReallocEvent> events;
    explicit ReallocHandler(ReallocScenario selected) : scenario(selected) {}
    void CallNewHandler(GuestAddress target, GuestMemory& memory,
        ReallocFull& state) override
    {
        events.push_back(ReallocRecord(target, state));
        if (scenario != ReallocScenario::HandlerRetry ||
            target != ReallocHandlerTarget || events.size() != 1u)
            throw std::runtime_error("unexpected CRT realloc handler");
        memory.WriteU8(0x54020u, 0x5au);
        ReallocMutateVolatile(state);
        state.r[9] = 0x1122334455667788ull;
        state.r[3] = 1u;
    }
};

realloc_family::Dependencies ReallocDeps(Services& stream,
    ReallocHeapBoundary& heap, ReallocHandler& handler,
    ReallocGuest& guest)
{
    return {{Dependencies(stream), heap, handler}, guest};
}

void ReallocSeed(test::GuestWindow& window,
    ReallocScenario scenario)
{
    Seed(window, Mode::LockedWrite);
    auto memory = window.Memory();
    memory.WriteU32(0x83245708u, ReallocHeapAddress);
    memory.WriteU32(0x832d3aecu,
        scenario == ReallocScenario::HandlerRetry ? 1u : 0u);
    memory.WriteU32(0x832d3ae8u,
        scenario == ReallocScenario::HandlerRetry ?
            ReallocHandlerTarget | 1u : 0u);
    memory.WriteU32(0x83215210u, 0u);
    memory.WriteU32(ErrorState + 352u, 200u);
    for (unsigned index = 0; index < 45u; ++index)
    {
        memory.WriteU32(ReallocTable + index * 8u,
            0xdead0000u + index);
        memory.WriteU32(ReallocTable + index * 8u + 4u, 0u);
    }
}

PPCContext ReallocInitial(ReallocScenario scenario)
{
    PPCContext context{};
    const auto gprs = crt_full_oracle::Gprs(context);
    const auto fprs = crt_full_oracle::Fprs(context);
    for (unsigned index = 0; index < 32u; ++index)
    {
        gprs[index]->u64 = 0x1122334400000000ull + index;
        fprs[index]->u64 = 0x3ff0000000000000ull + index;
    }
    context.r1.u64 = 0x8877665500000000ull | Stack;
    context.r13.u64 = 0xaabbccdd00000000ull | Environment;
    context.r3.u64 = scenario == ReallocScenario::NullAllocate ?
        0xaabbccdd00000000ull :
        (0xaabbccdd00000000ull | ReallocOldPointer);
    context.r4.u64 = 0x9988776600000000ull |
        (scenario == ReallocScenario::FreeZero ? 0u :
        scenario == ReallocScenario::Oversize ? 0xfffff100u : 64u);
    context.lr = 0xabcdef0181234567ull;
    context.ctr.u64 = 0x5566778899aabbccull;
    context.fpscr.csr = 0x9fc0u;
    context.xer.so = 1;
    context.xer.ca = 1;
    context.cr0.lt = 1;
    context.cr1.eq = 1;
    context.cr6.gt = 1;
    context.cr7.lt = 1;
    return context;
}

bool ReallocIndependent(ReallocScenario scenario,
    const PPCContext& context, const Services& stream,
    const ReallocGuest& guest, const ReallocHandler& handler)
{
    const auto error = stream.memory.ReadU32(0x83215210u);
    for (const auto& event : guest.events)
        if (event[1] != (0x8877665500000000ull | (Stack - 128u)))
            return false;
    switch (scenario)
    {
    case ReallocScenario::NullAllocate:
        return context.r3.u64 == ReallocNewPointer && error == 0u &&
            guest.events.size() == 1u &&
            guest.events[0][0] == 0x823acbd0u &&
            guest.events[0][2] == 0x823acafcu &&
            guest.events[0][3] == 0x9988776600000040ull &&
            handler.events.empty();
    case ReallocScenario::FreeZero:
        return context.r3.u64 == 0u && error == 0u &&
            guest.events.size() == 1u &&
            guest.events[0][0] == 0x823addc0u &&
            guest.events[0][2] == 0x823acb10u &&
            guest.events[0][3] ==
                (0xaabbccdd00000000ull | ReallocOldPointer) &&
            handler.events.empty();
    case ReallocScenario::Success:
        return context.r3.u64 == ReallocNewPointer && error == 0u &&
            guest.events.size() == 1u &&
            guest.events[0][0] == 0x827ccf80u &&
            guest.events[0][2] == 0x823acb44u &&
            guest.events[0][5] ==
                (0xaabbccdd00000000ull | ReallocOldPointer) &&
            guest.events[0][6] == 0x9988776600000040ull &&
            handler.events.empty();
    case ReallocScenario::StatusFallback:
        return context.r3.u64 == 0u && error == 8u &&
            context.xer.ca == 1u && guest.events.size() == 1u &&
            guest.events[0][0] == 0x827ccf80u &&
            handler.events.empty();
    case ReallocScenario::HandlerRetry:
        return context.r3.u64 == ReallocNewPointer && error == 0u &&
            guest.events.size() == 2u &&
            guest.events[0][0] == 0x827ccf80u &&
            guest.events[1][0] == 0x827ccf80u &&
            handler.events.size() == 1u &&
            handler.events[0][0] == ReallocHandlerTarget &&
            handler.events[0][1] ==
                (0x8877665500000000ull | (Stack - 224u)) &&
            handler.events[0][2] == 0x82b7fe8cu &&
            stream.memory.ReadU8(0x54020u) == 0x5au;
    case ReallocScenario::Oversize:
        return context.r3.u64 == 0u && error == 12u &&
            guest.events.empty() && handler.events.empty();
    }
    return false;
}

Services* realloc_stream_current = nullptr;
ReallocHeapBoundary* realloc_heap_current = nullptr;
ReallocHandler* realloc_handler_current = nullptr;
ReallocGuest* realloc_guest_current = nullptr;

void ReallocCheck(ReallocScenario scenario)
{
    test::GuestWindow original(ReallocRegions), recovered(ReallocRegions);
    ReallocSeed(original, scenario);
    ReallocSeed(recovered, scenario);
    Services expected(original, Mode::LockedWrite);
    Services actual(recovered, Mode::LockedWrite);
    ReallocHeapBoundary expected_heap, actual_heap;
    ReallocHandler expected_handler(scenario), actual_handler(scenario);
    ReallocGuest expected_guest(scenario), actual_guest(scenario);
    auto context = ReallocInitial(scenario);
    auto state = crt_full_oracle::FromPpc(context);
    active = &expected;
    realloc_stream_current = &expected;
    realloc_heap_current = &expected_heap;
    realloc_handler_current = &expected_handler;
    realloc_guest_current = &expected_guest;
    __imp__sub_823ACAD8(context, original.Bytes());
    active = nullptr;
    realloc_stream_current = nullptr;
    realloc_heap_current = nullptr;
    realloc_handler_current = nullptr;
    realloc_guest_current = nullptr;
    if (!ReallocIndependent(scenario, context, expected,
        expected_guest, expected_handler))
    {
        std::fprintf(stderr, "EXPECT reallocation case=%u r3=%llX "
            "errno=%u CA=%u guest=%zu handler=%zu\n",
            static_cast<unsigned>(scenario),
            static_cast<unsigned long long>(context.r3.u64),
            expected.memory.ReadU32(0x83215210u), context.xer.ca,
            expected_guest.events.size(), expected_handler.events.size());
        throw std::runtime_error("independent reallocation outcome");
    }
    if (!realloc_family::Apply(0x823acAD8u, actual.memory,
        ReallocDeps(actual, actual_heap, actual_handler, actual_guest), state))
        throw std::runtime_error("missing selected reallocation entry");
    const auto before = crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after = crt_full_oracle::Snapshot(state);
    const bool same_state = before == after;
    const bool same_ram = original.EqualCommitted(recovered);
    if (!same_state || !same_ram || expected.events != actual.events ||
        expected.traps != actual.traps ||
        expected_guest.events != actual_guest.events ||
        expected_handler.events != actual_handler.events)
    {
        std::fprintf(stderr, "FAIL reallocation case=%u state=%u RAM=%u "
            "guest=%zu/%zu handler=%zu/%zu stream=%zu/%zu\n",
            static_cast<unsigned>(scenario), same_state, same_ram,
            expected_guest.events.size(), actual_guest.events.size(),
            expected_handler.events.size(), actual_handler.events.size(),
            expected.events.size(), actual.events.size());
        for (unsigned index = 0; index < before.size(); ++index)
            if (before[index] != after[index])
                std::fprintf(stderr, " state[%u]=%llX/%llX\n", index,
                    static_cast<unsigned long long>(before[index]),
                    static_cast<unsigned long long>(after[index]));
        throw std::runtime_error("reallocation selected context differs");
    }
}
} // namespace

void OriginalReallocationLower(GuestAddress entry, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    if (!realloc_family::ApplyLower(entry, realloc_stream_current->memory,
        ReallocDeps(*realloc_stream_current, *realloc_heap_current,
            *realloc_handler_current, *realloc_guest_current), state))
        throw std::runtime_error("missing reallocation original lower");
    crt_full_oracle::ToPpc(context, state);
}

void OriginalReallocationGuest(GuestAddress entry, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    realloc_guest_current->CallLower(entry, realloc_stream_current->memory,
        state);
    crt_full_oracle::ToPpc(context, state);
}

void OriginalReallocationHandler(GuestAddress target,
    PPCContext& context, std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    realloc_handler_current->CallNewHandler(target,
        realloc_stream_current->memory, state);
    crt_full_oracle::ToPpc(context, state);
}

int main()
{
    try
    {
        for (const auto scenario : ReallocScenarios)
            ReallocCheck(scenario);
        std::printf("PASS crt-reallocation-context %zu original PPC cases\n",
            ReallocScenarios.size());
        std::puts("LIMIT four actual PPC bodies; accepted FD78/822CA100 "
            "selected adapters and mutable ACBD0/ADDC0/CCF80 guest "
            "boundaries; unselected CR, fault, MMIO, concurrency and "
            "runtime open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
