#define main BufferCallersBlockFixtureMain
#include "crt_close_block_output_context_oracle.cpp"
#undef main

#include "lo_semantics/crt_close_buffer_callers_context.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/recovery_abi.h"

namespace buffer_callers_oracle
{
namespace caller = crt_close_buffer_callers_context;
using Full = caller::Registers;
constexpr GuestAddress Node = 0x56000u;
constexpr GuestAddress Reader = 0x57000u;
enum class Route { EmptyNode, WriteNode, EmptyChain, NullClose };
struct Case { const char* name; GuestAddress entry; Route route; };
constexpr std::array Cases{
    Case{"empty-node", 0x82bd0898u, Route::EmptyNode},
    Case{"buffered-node", 0x82bd0898u, Route::WriteNode},
    Case{"empty-recursive-chain", 0x82bd0df8u, Route::EmptyChain},
    Case{"null-close-tail", 0x82bd1200u, Route::NullClose}};

struct RecursiveGuest final : crt_close_recursive_buffer_context::GuestServices
{
    void CallIndirect(GuestAddress, GuestMemory&, Full&) override
    { throw std::runtime_error("unexpected recursive allocation"); }
};

RecursiveGuest* original_recursive = nullptr;

caller::Dependencies Deps(Services& stream, IndexService& index, Host& host,
    wrapper_oracle::WrapperUnlock& unlock,
    block_output_oracle::BlockExtra& extra, RecursiveGuest& recursive)
{
    return {recursive, block_output_oracle::Deps(stream, index, host,
        unlock, extra)};
}

void Seed(GuestWindow& window, Route route)
{
    block_output_oracle::SeedBlock(window);
    auto memory = window.Memory();
    memory.WriteU32(Node, block_output_oracle::Input);
    memory.WriteU32(Node + 4u, route == Route::WriteNode ? 3u : 0u);
    memory.WriteU32(Node + 12u, 0u);
    memory.WriteU32(Reader + 4u, Node);
    memory.WriteU8(Reader + 24u, 0u);
    memory.WriteU8(close_shared_oracle::ModeString, 'w');
    memory.WriteU8(close_shared_oracle::ModeString + 1u, 0u);
}

PPCContext Initial(Route route)
{
    auto context = block_output_oracle::InitialBlock(
        block_output_oracle::BlockRoute::Buffered);
    context.r3.u64 = 0xaabbccdd00000000ull | Reader;
    context.r4.u64 = 0x5566778800000000ull | Node;
    context.r5.u64 = 0x9988776600000000ull | Stream;
    if (route == Route::EmptyChain)
        context.r4.u64 = 0x9988776600000000ull | Stream;
    if (route == Route::NullClose)
    {
        context.r4.u64 = 0u;
        context.r5.u64 = close_shared_oracle::ModeString;
    }
    return context;
}

void Check(const Case& item)
{
    GuestWindow original(OpenRegions), recovered(OpenRegions);
    Seed(original, item.route);
    Seed(recovered, item.route);
    Services expected(original, Mode::LockedWrite);
    Services actual(recovered, Mode::LockedWrite);
    IndexService expected_index, actual_index;
    Host expected_host(Scenario::BinarySuccess);
    Host actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock, actual_unlock;
    block_output_oracle::BlockExtra expected_extra, actual_extra;
    RecursiveGuest expected_recursive, actual_recursive;
    auto context = Initial(item.route);
    auto state = crt_full_oracle::FromPpc(context);
    current = &expected;
    active = &expected;
    current_index = &expected_index;
    current_host = &expected_host;
    wrapper_oracle::original_unlock = &expected_unlock;
    close_shared_oracle::active_extra = &expected_extra;
    block_output_oracle::original_extra = &expected_extra;
    original_recursive = &expected_recursive;
    switch (item.entry)
    {
    case 0x82bd0898u: __imp__sub_82BD0898(context, original.Bytes()); break;
    case 0x82bd0df8u: __imp__sub_82BD0DF8(context, original.Bytes()); break;
    case 0x82bd1200u: __imp__sub_82BD1200(context, original.Bytes()); break;
    default: throw std::runtime_error("unknown upper close entry");
    }
    current = nullptr;
    active = nullptr;
    current_index = nullptr;
    current_host = nullptr;
    wrapper_oracle::original_unlock = nullptr;
    close_shared_oracle::active_extra = nullptr;
    block_output_oracle::original_extra = nullptr;
    original_recursive = nullptr;
    if (!caller::Apply(item.entry, actual.memory,
        Deps(actual, actual_index, actual_host, actual_unlock,
            actual_extra, actual_recursive), state))
        throw std::runtime_error("missing upper close body");
    const auto before = crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after = crt_full_oracle::Snapshot(state);
    if (before != after || !original.EqualCommitted(recovered) ||
        expected.events != actual.events ||
        expected.traps != actual.traps ||
        expected_extra.events != actual_extra.events ||
        expected_extra.output_events != actual_extra.output_events ||
        expected_index.events != actual_index.events ||
        expected_host.events != actual_host.events ||
        expected_unlock.events != actual_unlock.events)
    {
        for (unsigned i = 0; i < before.size(); ++i)
            if (before[i] != after[i])
                std::fprintf(stderr, "%s state[%u] %llx/%llx\n", item.name,
                    i, static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("upper full state/RAM/callback mismatch");
    }
    if (item.route == Route::EmptyNode &&
        (context.r3.u32 != 1u || !expected.events.empty()))
        throw std::runtime_error("empty node did not return one");
    if (item.route == Route::WriteNode &&
        (context.r3.u32 != 1u ||
            expected.memory.ReadU32(Stream + 4u) != 9u ||
            expected_extra.events.size() != 2u))
        throw std::runtime_error("buffered node did not write");
    if (item.route == Route::EmptyChain && context.r3.u32 != 1u)
        throw std::runtime_error("empty recursive chain did not finish");
    if (item.route == Route::NullClose &&
        (context.r3.u32 != 0u ||
            expected.memory.ReadU32(0x83215210u) != 22u))
        throw std::runtime_error("null close did not reach shared lower");
}
} // namespace buffer_callers_oracle

void OriginalCallerSave28(PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned i = 28u; i <= 31u; ++i)
        lo::semantic::gpu::recovery_abi::WriteU64(current->memory,
            lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - i)), gprs[i]->u64);
    current->memory.WriteU32(lo::semantic::gpu::recovery_abi::Address(
        context.r1.u64 - 8u), context.r12.u32);
}

void OriginalCallerRestore28(PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned i = 28u; i <= 31u; ++i)
        gprs[i]->u64 = lo::semantic::gpu::recovery_abi::ReadU64(
            current->memory, lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - i)));
    context.r12.u64 = current->memory.ReadU32(
        lo::semantic::gpu::recovery_abi::Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}

void OriginalCallerRecursive(PPCContext& context, std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    if (!crt_close_recursive_buffer_context::Apply(0x82bd0c18u,
        current->memory, *buffer_callers_oracle::original_recursive, state))
        throw std::runtime_error("missing selected recursive lower");
    crt_full_oracle::ToPpc(context, state);
}

void OriginalCallerBulk(PPCContext& context, std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    auto lower = crt_context_adapter::ToStream(state);
    if (!crt_stream_bulk_close_routes::Apply(0x82b85d88u,
        current->memory, close_shared_oracle::Deps(*current,
            *current_index, *current_host,
            *wrapper_oracle::original_unlock,
            *close_shared_oracle::active_extra).close, lower))
        throw std::runtime_error("missing accepted bulk close lower");
    crt_context_adapter::FromStream(state, lower);
    crt_full_oracle::ToPpc(context, state);
}

int main()
{
    for (const auto& item : buffer_callers_oracle::Cases)
    {
        try { buffer_callers_oracle::Check(item); }
        catch (const std::exception& error)
        { std::fprintf(stderr, "%s: %s\n", item.name, error.what()); return 1; }
    }
    std::puts("PASS crt-close-buffer-callers-context 4 focused actual PPC cases");
    std::puts("LIMIT selected accepted recursive/bulk and guest/native lower ABI; fault/MMIO/concurrency/runtime unvalidated");
    return 0;
}
