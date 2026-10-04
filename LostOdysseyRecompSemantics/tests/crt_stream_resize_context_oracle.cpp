// Appended after the six exact original PPC bodies in the validation pin.
#include "crt_full_context_oracle_fixture.h"
#include "heap_block_query_context_fixture.h"
#include "lo_semantics/crt_stream_resize_context.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace resize_family = crt_stream_resize_context;
namespace query_fixture = heap_block_query_context_fixture;
using ResizeFull = resize_family::Registers;
using ResizeHeap = heap_block_query_context::Registers;

constexpr std::array<test::Region, 6> ResizeRegions{{
    {0u, 0x120000u}, {0x831e0000u, 0x10000u},
    {0x83214000u, 0x3000u}, {0x83245000u, 0x1000u},
    {0x832d3000u, 0x2000u}, {0x83378000u, 0x3000u}}};
enum class ResizeScenario {NullSize, BlockSize, Overflow, GrowExisting};
constexpr std::array ResizeScenarios{ResizeScenario::NullSize,
    ResizeScenario::BlockSize, ResizeScenario::Overflow,
    ResizeScenario::GrowExisting};
using ResizeEvent = std::array<std::uint64_t, 7>;
constexpr std::uint64_t Reallocated =
    0x7766554400000000ull | query_fixture::Payload;

ResizeHeap ResizeToHeap(const ResizeFull& state)
{
    ResizeHeap lower{};
    lower.r = state.r;
    lower.lr = state.lr;
    lower.ctr = state.ctr;
    lower.f0_bits = state.fpr_bits[0];
    lower.f1_bits = state.fpr_bits[1];
    lower.f13_bits = state.fpr_bits[13];
    lower.f30_bits = state.fpr_bits[30];
    lower.f31_bits = state.fpr_bits[31];
    lower.cached_fp_control = state.cached_fp_control;
    lower.xer_so = state.xer_so;
    lower.xer_ca = state.xer_ca;
    lower.cr0 = {state.cr0.lt, state.cr0.gt,
        state.cr0.eq, state.cr0.so};
    lower.cr6 = {state.cr6.lt, state.cr6.gt,
        state.cr6.eq, state.cr6.so};
    return lower;
}

void ResizeFromHeap(ResizeFull& state, const ResizeHeap& lower)
{
    state.r = lower.r;
    state.lr = lower.lr;
    state.ctr = lower.ctr;
    state.fpr_bits[0] = lower.f0_bits;
    state.fpr_bits[1] = lower.f1_bits;
    state.fpr_bits[13] = lower.f13_bits;
    state.fpr_bits[30] = lower.f30_bits;
    state.fpr_bits[31] = lower.f31_bits;
    state.cached_fp_control = lower.cached_fp_control;
    state.xer_so = lower.xer_so;
    state.xer_ca = lower.xer_ca;
    state.cr0 = {lower.cr0.lt, lower.cr0.gt,
        lower.cr0.eq, lower.cr0.un};
    state.cr6 = {lower.cr6.lt, lower.cr6.gt,
        lower.cr6.eq, lower.cr6.un};
}

struct ResizeHeapBoundary final : heap_allocation_context::BoundaryServices
{
    void CallDirect(GuestAddress, GuestMemory&, ResizeHeap&) override
    { throw std::runtime_error("unexpected resize heap guest call"); }
    void CallNative(GuestAddress, GuestMemory&, ResizeHeap&) override
    { throw std::runtime_error("unexpected resize heap native call"); }
};

struct ResizeHandler final : crt_record_allocation_context::HandlerServices
{
    void CallNewHandler(GuestAddress, GuestMemory&, ResizeFull&) override
    { throw std::runtime_error("unexpected resize new handler"); }
};

struct ResizeHeapNative final : heap_block_query_context::NativeServices
{
    void CallNative(GuestAddress, GuestMemory&, ResizeHeap&) override
    { throw std::runtime_error("unexpected resize heap query native"); }
};

struct ResizeGuest final : crt_reallocation_context::GuestServices
{
    std::vector<ResizeEvent> events;
    void CallLower(GuestAddress entry, GuestMemory&, ResizeFull& state) override
    {
        events.push_back({entry, state.r[1], state.lr, state.r[3],
            state.r[4], state.r[5], state.r[6]});
        if (entry != 0x827ccf80u ||
            static_cast<GuestAddress>(state.r[5]) !=
                query_fixture::Payload ||
            static_cast<GuestAddress>(state.r[6]) != 120u)
            throw std::runtime_error("unexpected resize reallocation guest");
        state.r[3] = Reallocated;
    }
};

resize_family::Dependencies ResizeDeps(Services& stream,
    ResizeHeapBoundary& heap, ResizeHandler& handler,
    ResizeHeapNative& heap_native, ResizeGuest& guest)
{
    return {{{Dependencies(stream), heap, handler}, guest}, heap_native};
}

PPCContext ResizeInitial(ResizeScenario scenario)
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
    context.r3.u64 = (scenario == ResizeScenario::BlockSize ||
        scenario == ResizeScenario::GrowExisting)
        ? (0xaabbccdd00000000ull | query_fixture::Payload) : 0u;
    context.r4.u64 = 0x9988776600000000ull |
        (scenario == ResizeScenario::Overflow ? 4u : 120u);
    context.r5.u64 = 0x5566778800000000ull |
        (scenario == ResizeScenario::Overflow ? 0x40000000u : 1u);
    context.lr = 0xabcdef0181234567ull;
    context.ctr.u64 = 0x5566778899aabbccull;
    context.fpscr.csr = 0x9fc0u;
    context.xer.so = 1;
    context.xer.ca = 1;
    context.cr0.lt = 1;
    context.cr6.gt = 1;
    return context;
}

void ResizeSeed(test::GuestWindow& window)
{
    Seed(window, Mode::LockedWrite);
    auto memory = window.Memory();
    memory.WriteU32(0x83245708u, query_fixture::Heap);
    query_fixture::Seed(memory, query_fixture::Mode::Small);
    for (unsigned index = 104u; index < 120u; ++index)
        memory.WriteU8(query_fixture::Payload + index, 0x5au);
}

bool ResizeIndependent(ResizeScenario scenario, const PPCContext& context,
    const Services& stream, const ResizeGuest& guest)
{
    const auto error = stream.memory.ReadU32(0x83215210u);
    if (scenario == ResizeScenario::NullSize)
        return context.r3.u64 == UINT64_MAX && error == 22u &&
            stream.traps == 1u && guest.events.empty();
    if (scenario == ResizeScenario::BlockSize)
        return context.r3.u64 == 104u && error == 0u &&
            stream.traps == 0u && guest.events.empty();
    if (scenario == ResizeScenario::Overflow)
        return context.r3.u64 == 0u && error == 12u &&
            stream.traps == 1u && guest.events.empty();
    if (context.r3.u64 != Reallocated || error != 0u ||
        stream.traps != 0u || guest.events.size() != 1u ||
        guest.events[0][0] != 0x827ccf80u)
        return false;
    for (unsigned index = 104u; index < 120u; ++index)
        if (stream.memory.ReadU8(query_fixture::Payload + index) != 0u)
            return false;
    return true;
}

Services* resize_stream_current = nullptr;
ResizeHeapBoundary* resize_heap_current = nullptr;
ResizeHandler* resize_handler_current = nullptr;
ResizeHeapNative* resize_native_current = nullptr;
ResizeGuest* resize_guest_current = nullptr;

void ResizeCheck(ResizeScenario scenario)
{
    test::GuestWindow original(ResizeRegions), recovered(ResizeRegions);
    ResizeSeed(original);
    ResizeSeed(recovered);
    Services expected(original, Mode::LockedWrite);
    Services actual(recovered, Mode::LockedWrite);
    ResizeHeapBoundary expected_heap, actual_heap;
    ResizeHandler expected_handler, actual_handler;
    ResizeHeapNative expected_native, actual_native;
    ResizeGuest expected_guest, actual_guest;
    auto context = ResizeInitial(scenario);
    auto state = crt_full_oracle::FromPpc(context);
    const auto entry = scenario == ResizeScenario::NullSize ||
        scenario == ResizeScenario::BlockSize ? 0x82b82100u : 0x82b7d330u;
    active = &expected;
    resize_stream_current = &expected;
    resize_heap_current = &expected_heap;
    resize_handler_current = &expected_handler;
    resize_native_current = &expected_native;
    resize_guest_current = &expected_guest;
    if (entry == 0x82b82100u)
        __imp__sub_82B82100(context, original.Bytes());
    else __imp__sub_82B7D330(context, original.Bytes());
    active = nullptr;
    resize_stream_current = nullptr;
    resize_heap_current = nullptr;
    resize_handler_current = nullptr;
    resize_native_current = nullptr;
    resize_guest_current = nullptr;
    if (!ResizeIndependent(scenario, context, expected, expected_guest))
    {
        std::fprintf(stderr, "EXPECT resize scenario=%u r3=%llX "
            "errno=%u traps=%u guest=%zu\n",
            static_cast<unsigned>(scenario),
            static_cast<unsigned long long>(context.r3.u64),
            expected.memory.ReadU32(0x83215210u), expected.traps,
            expected_guest.events.size());
        throw std::runtime_error("independent resize outcome");
    }
    if (!resize_family::Apply(entry, actual.memory,
        ResizeDeps(actual, actual_heap, actual_handler,
            actual_native, actual_guest), state))
        throw std::runtime_error("missing selected resize entry");
    const auto before = crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after = crt_full_oracle::Snapshot(state);
    const bool same_state = before == after;
    const bool same_ram = original.EqualCommitted(recovered);
    if (!same_state || !same_ram || expected_guest.events !=
        actual_guest.events || expected.events != actual.events ||
        expected.traps != actual.traps)
    {
        std::fprintf(stderr, "FAIL resize scenario=%u state=%u RAM=%u "
            "guest=%zu/%zu stream=%zu/%zu traps=%u/%u\n",
            static_cast<unsigned>(scenario), same_state, same_ram,
            expected_guest.events.size(), actual_guest.events.size(),
            expected.events.size(), actual.events.size(),
            expected.traps, actual.traps);
        for (unsigned index = 0; index < before.size(); ++index)
            if (before[index] != after[index])
                std::fprintf(stderr, " state[%u]=%llX/%llX\n", index,
                    static_cast<unsigned long long>(before[index]),
                    static_cast<unsigned long long>(after[index]));
        throw std::runtime_error("resize selected context differs");
    }
}
} // namespace

void OriginalResizeAccepted(GuestAddress entry, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    const auto dependencies = ResizeDeps(*resize_stream_current,
        *resize_heap_current, *resize_handler_current,
        *resize_native_current, *resize_guest_current);
    if (!crt_record_allocation_context::ApplyAcceptedLower(entry,
        resize_stream_current->memory,
        dependencies.reallocation.allocation, state) &&
        !crt_record_allocation_context::Apply(entry,
            resize_stream_current->memory,
            dependencies.reallocation.allocation, state) &&
        !crt_reallocation_context::ApplyLower(entry,
            resize_stream_current->memory,
            dependencies.reallocation, state))
        throw std::runtime_error("missing original resize accepted lower");
    crt_full_oracle::ToPpc(context, state);
}

void OriginalResizeGuest(GuestAddress entry, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    resize_guest_current->CallLower(entry, resize_stream_current->memory,
        state);
    crt_full_oracle::ToPpc(context, state);
}

void OriginalResizeNative(GuestAddress entry, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    auto lower = ResizeToHeap(state);
    resize_native_current->CallNative(entry, resize_stream_current->memory,
        lower);
    ResizeFromHeap(state, lower);
    crt_full_oracle::ToPpc(context, state);
}

int main()
{
    try
    {
        for (const auto scenario : ResizeScenarios) ResizeCheck(scenario);
        std::printf("PASS crt-stream-resize-context %zu original PPC cases\n",
            ResizeScenarios.size());
        std::puts("LIMIT six actual PPC bodies; accepted FD78/FEC0 selected "
            "adapters and mutable reallocation guest/native boundaries; "
            "unselected CR, fault, MMIO, concurrency and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
