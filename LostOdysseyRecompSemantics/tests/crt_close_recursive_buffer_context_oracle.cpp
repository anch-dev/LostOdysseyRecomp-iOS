// Appended after the three exact original PPC bodies in the validation pin.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_close_recursive_buffer_context.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
namespace recursive_family = crt_close_recursive_buffer_context;
using RecursiveFull = recursive_family::Registers;
constexpr GuestAddress RecursiveReader = 0x30000u;
constexpr GuestAddress RecursiveOldNode = 0x31000u;
constexpr GuestAddress RecursiveOldBuffer = 0x32000u;
constexpr GuestAddress RecursiveNewNode = 0x40000u;
constexpr GuestAddress RecursiveNewBuffer = 0x41000u;
constexpr GuestAddress RecursiveTableGlobal = 0x832df554u;
constexpr GuestAddress RecursiveTable = 0x83216624u;
constexpr GuestAddress RecursiveVtable = 0x34000u;
constexpr GuestAddress RecursiveTarget = 0x2a00u;
constexpr std::array<test::Region, 3> RecursiveRegions{{
    {0u, 0x120000u}, {0x83216000u, 0x1000u},
    {0x832df000u, 0x1000u}}};
enum class RecursiveScenario
{Empty, NoGrowth, Growth, FirstFailure, SecondFailure};
constexpr std::array RecursiveScenarios{RecursiveScenario::Empty,
    RecursiveScenario::NoGrowth, RecursiveScenario::Growth,
    RecursiveScenario::FirstFailure, RecursiveScenario::SecondFailure};
using RecursiveEvent = std::array<std::uint64_t, 8>;

struct RecursiveGuest final : recursive_family::GuestServices
{
    RecursiveScenario scenario;
    std::vector<RecursiveEvent> events;
    explicit RecursiveGuest(RecursiveScenario selected) : scenario(selected) {}
    void CallIndirect(GuestAddress target, GuestMemory& memory,
        RecursiveFull& state) override
    {
        events.push_back({target, state.r[1], state.lr, state.r[3],
            state.r[4], state.r[5], state.ctr, state.fpr_bits[7]});
        if (target != RecursiveTarget || events.size() > 2u)
            throw std::runtime_error("unexpected recursive guest target");
        memory.WriteU32(0x54000u +
            static_cast<GuestAddress>((events.size() - 1u) * 4u),
            static_cast<std::uint32_t>(state.r[5]));
        state.r[8] ^= 0x123456789abcdef0ull;
        state.fpr_bits[7] ^= 0x180u;
        state.cached_fp_control ^= 0x40u;
        state.cr1 = {0, 1, 0, state.xer_so};
        state.cr7 = {1, 0, 0, state.xer_so};
        state.xer_ca ^= 1u;
        if (scenario == RecursiveScenario::FirstFailure &&
            events.size() == 1u)
        { state.r[3] = 0u; return; }
        if (scenario == RecursiveScenario::SecondFailure &&
            events.size() == 2u)
        { state.r[3] = 0u; return; }
        state.r[3] = events.size() == 1u ?
            (0x9988776600000000ull | RecursiveNewNode) :
            (0x7766554400000000ull | RecursiveNewBuffer);
    }
};

void RecursiveSeed(test::GuestWindow& window,
    RecursiveScenario scenario)
{
    window.Fill(0xa5u);
    auto memory = window.Memory();
    memory.WriteU32(RecursiveTableGlobal, 0u);
    memory.WriteU32(RecursiveTable, RecursiveVtable);
    memory.WriteU32(RecursiveVtable, RecursiveTarget | 1u);
    memory.WriteU32(RecursiveReader, RecursiveOldNode);
    memory.WriteU32(RecursiveReader + 16u, 0u);
    memory.WriteU8(RecursiveReader + 24u,
        scenario == RecursiveScenario::Empty ||
        scenario == RecursiveScenario::FirstFailure ||
        scenario == RecursiveScenario::SecondFailure ? 0u : 7u);
    memory.WriteU8(RecursiveReader + 25u, 0x41u);
    memory.WriteU32(RecursiveOldNode, RecursiveOldBuffer);
    memory.WriteU32(RecursiveOldNode + 4u,
        scenario == RecursiveScenario::Growth ? 1u : 0u);
    memory.WriteU32(RecursiveOldNode + 8u,
        scenario == RecursiveScenario::Growth ? 1u : 4u);
    memory.WriteU32(RecursiveOldNode + 12u, 0u);
}

PPCContext RecursiveInitial(RecursiveScenario scenario)
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
    context.r3.u64 = 0xaabbccdd00000000ull | RecursiveReader;
    if (scenario == RecursiveScenario::FirstFailure ||
        scenario == RecursiveScenario::SecondFailure)
    {
        context.r4.u64 = 0u;
        context.r5.u64 = 4u;
    }
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

bool RecursiveIndependent(RecursiveScenario scenario,
    const PPCContext& context, const GuestMemory& memory,
    const RecursiveGuest& guest)
{
    const auto reader = memory.ReadU32(RecursiveReader);
    if (scenario == RecursiveScenario::Empty)
        return context.r3.u64 ==
            (0xaabbccdd00000000ull | RecursiveReader) &&
            memory.ReadU8(RecursiveReader + 24u) == 0u &&
            guest.events.empty();
    if (scenario == RecursiveScenario::NoGrowth)
        return context.r3.u64 ==
            (0xaabbccdd00000000ull | RecursiveReader) &&
            reader == RecursiveOldNode &&
            memory.ReadU32(RecursiveOldNode + 4u) == 1u &&
            memory.ReadU32(RecursiveReader + 16u) == RecursiveOldBuffer &&
            memory.ReadU8(RecursiveOldBuffer) == 0x82u &&
            memory.ReadU8(RecursiveReader + 24u) == 0u &&
            guest.events.empty();
    if (scenario == RecursiveScenario::Growth)
        return context.r3.u64 ==
            (0xaabbccdd00000000ull | RecursiveReader) &&
            reader == RecursiveNewNode &&
            memory.ReadU32(RecursiveOldNode + 12u) == RecursiveNewNode &&
            memory.ReadU32(RecursiveNewNode) == RecursiveNewBuffer &&
            memory.ReadU32(RecursiveNewNode + 4u) == 1u &&
            memory.ReadU32(RecursiveNewNode + 8u) == 2u &&
            memory.ReadU8(RecursiveNewBuffer) == 0x82u &&
            guest.events.size() == 2u &&
            guest.events[0][1] ==
                (0x8877665500000000ull | (Stack - 240u)) &&
            guest.events[0][2] == 0x82bd080cu &&
            guest.events[1][2] == 0x82bd0868u;
    if (scenario == RecursiveScenario::FirstFailure)
        return context.r3.u64 == 0u && reader == RecursiveOldNode &&
            memory.ReadU32(RecursiveTableGlobal) == RecursiveTable &&
            guest.events.size() == 1u &&
            guest.events[0][1] ==
                (0x8877665500000000ull | (Stack - 128u)) &&
            guest.events[0][3] ==
                (0xffffffff00000000ull | RecursiveTable) &&
            guest.events[0][4] == 16u && guest.events[0][5] == 36u;
    return context.r3.u64 == 0u && reader == RecursiveOldNode &&
        memory.ReadU32(RecursiveNewNode) == 0u &&
        memory.ReadU32(RecursiveNewNode + 8u) == 4u &&
        guest.events.size() == 2u &&
        guest.events[1][3] == RecursiveTable &&
        guest.events[1][4] == 4u && guest.events[1][5] == 65u;
}

GuestMemory* recursive_original_memory = nullptr;
RecursiveGuest* recursive_guest_current = nullptr;

void RecursiveCheck(RecursiveScenario scenario)
{
    test::GuestWindow original(RecursiveRegions), recovered(RecursiveRegions);
    RecursiveSeed(original, scenario);
    RecursiveSeed(recovered, scenario);
    auto original_memory = original.Memory();
    auto recovered_memory = recovered.Memory();
    RecursiveGuest expected_guest(scenario), actual_guest(scenario);
    auto context = RecursiveInitial(scenario);
    auto state = crt_full_oracle::FromPpc(context);
    const auto entry = scenario == RecursiveScenario::FirstFailure ||
        scenario == RecursiveScenario::SecondFailure ?
        0x82bd07d8u : 0x82bd0c18u;
    recursive_original_memory = &original_memory;
    recursive_guest_current = &expected_guest;
    if (entry == 0x82bd07d8u)
        __imp__sub_82BD07D8(context, original.Bytes());
    else __imp__sub_82BD0C18(context, original.Bytes());
    recursive_original_memory = nullptr;
    recursive_guest_current = nullptr;
    if (!RecursiveIndependent(scenario, context,
        original_memory, expected_guest))
    {
        std::fprintf(stderr, "EXPECT recursive case=%u r3=%llX "
            "reader=%08X node=%08X events=%zu\n",
            static_cast<unsigned>(scenario),
            static_cast<unsigned long long>(context.r3.u64),
            original_memory.ReadU32(RecursiveReader),
            original_memory.ReadU32(RecursiveNewNode),
            expected_guest.events.size());
        throw std::runtime_error("independent recursive outcome");
    }
    if (!recursive_family::Apply(entry, recovered_memory,
        actual_guest, state))
        throw std::runtime_error("missing recursive entry");
    const auto before = crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after = crt_full_oracle::Snapshot(state);
    const bool same_state = before == after;
    const bool same_ram = original.EqualCommitted(recovered);
    if (!same_state || !same_ram ||
        expected_guest.events != actual_guest.events)
    {
        std::fprintf(stderr, "FAIL recursive case=%u state=%u RAM=%u "
            "events=%zu/%zu\n", static_cast<unsigned>(scenario),
            same_state, same_ram, expected_guest.events.size(),
            actual_guest.events.size());
        for (unsigned index = 0; index < before.size(); ++index)
            if (before[index] != after[index])
                std::fprintf(stderr, " state[%u]=%llX/%llX\n", index,
                    static_cast<unsigned long long>(before[index]),
                    static_cast<unsigned long long>(after[index]));
        throw std::runtime_error("recursive selected context differs");
    }
}
} // namespace

void OriginalRecursiveSave(unsigned first, PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned index = first; index <= 31u; ++index)
        lo::semantic::gpu::recovery_abi::WriteU64(*recursive_original_memory,
            lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - index)),
            gprs[index]->u64);
    recursive_original_memory->WriteU32(
        lo::semantic::gpu::recovery_abi::Address(context.r1.u64 - 8u),
        context.r12.u32);
}

void OriginalRecursiveRestore(unsigned first, PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned index = first; index <= 31u; ++index)
        gprs[index]->u64 = lo::semantic::gpu::recovery_abi::ReadU64(
            *recursive_original_memory,
            lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - index)));
    context.r12.u64 = recursive_original_memory->ReadU32(
        lo::semantic::gpu::recovery_abi::Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}

void OriginalRecursiveIndirect(GuestAddress target,
    PPCContext& context, std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    recursive_guest_current->CallIndirect(target,
        *recursive_original_memory, state);
    crt_full_oracle::ToPpc(context, state);
}

int main()
{
    try
    {
        for (const auto scenario : RecursiveScenarios)
            RecursiveCheck(scenario);
        std::printf("PASS crt-close-recursive-buffer-context %zu "
            "original PPC cases\n", RecursiveScenarios.size());
        std::puts("LIMIT three actual PPC bodies; mutable indirect guest "
            "allocator, unselected CR, fault, MMIO, concurrency and "
            "runtime open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
