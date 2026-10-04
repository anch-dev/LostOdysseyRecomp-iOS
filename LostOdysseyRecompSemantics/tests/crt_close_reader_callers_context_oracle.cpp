#pragma push_macro("main")
#undef main
#define main ReaderCallersSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main
#pragma pop_macro("main")

#include "lo_semantics/crt_close_reader_callers_context.h"
#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"

namespace reader_callers_oracle
{
namespace family = crt_close_reader_callers_context;
using Full = family::Registers;
constexpr GuestAddress Reader = 0x57000u;
constexpr GuestAddress OldNode = 0x58000u;
constexpr GuestAddress OldData = 0x59000u;
constexpr GuestAddress NewNode = 0x5a000u;
constexpr GuestAddress NewData = 0x5b000u;
constexpr GuestAddress Source = 0x5c000u;
constexpr GuestAddress TableGlobal = 0x832df554u;
constexpr GuestAddress Table = 0x83216624u;
constexpr GuestAddress Vtable = 0x34000u;
constexpr GuestAddress Target = 0x2a00u;
constexpr std::array<test::Region, 8> ReaderRegions = []
{
    std::array<test::Region, 8> regions{};
    for (unsigned i = 0; i < OpenRegions.size(); ++i)
        regions[i] = OpenRegions[i];
    regions[7] = {0x832df000u, 0x1000u};
    return regions;
}();
enum class Route { CopySuccess, SecondAllocationFailure, NullName };
struct Case { const char* name; GuestAddress entry; Route route; };
constexpr std::array Cases{
    Case{"allocated-copy", 0x82bd0a60u, Route::CopySuccess},
    Case{"second-allocation-fallback", 0x82bd0a60u,
        Route::SecondAllocationFailure},
    Case{"null-name-close", 0x82bd0d28u, Route::NullName}};
using Event = std::array<std::uint64_t, 9>;

struct Guest final : crt_close_recursive_buffer_context::GuestServices
{
    Route route;
    std::vector<Event> events;
    explicit Guest(Route selected) : route(selected) {}
    void CallIndirect(GuestAddress target, GuestMemory& memory,
        Full& state) override
    {
        events.push_back({target, state.r[1], state.lr, state.r[3],
            state.r[4], state.r[5], state.ctr, state.fpr_bits[7],
            state.r[10]});
        if (target != Target || events.size() > 2u)
            throw std::runtime_error("unexpected allocator target");
        memory.WriteU32(0x53000u +
            static_cast<GuestAddress>(4u * (events.size() - 1u)),
            static_cast<std::uint32_t>(state.r[5]));
        state.r[10] ^= 0x123456789abcdef0ull;
        state.fpr_bits[7] ^= 0x100u;
        state.cr1.gt ^= 1u;
        state.cr7.eq ^= 1u;
        state.xer_ca ^= 1u;
        if (events.size() == 1u)
            state.r[3] = 0xaabbccdd00000000ull | NewNode;
        else state.r[3] = route == Route::SecondAllocationFailure ? 0u :
            (0x9988776600000000ull | NewData);
    }
};

Guest* original_guest = nullptr;
close_shared_oracle::SharedExtra* original_extra = nullptr;

family::Dependencies Deps(Services& stream, IndexService& index,
    Host& host, wrapper_oracle::WrapperUnlock& unlock,
    close_shared_oracle::SharedExtra& extra, Guest& guest)
{
    return {guest, close_shared_oracle::Deps(stream, index, host,
        unlock, extra)};
}

void Seed(GuestWindow& window)
{
    close_shared_oracle::SeedShared(window,
        close_shared_oracle::Route::NullPath);
    auto memory = window.Memory();
    memory.WriteU32(TableGlobal, 0u);
    memory.WriteU32(Table, Vtable);
    memory.WriteU32(Vtable, Target | 1u);
    memory.WriteU32(Reader, OldNode);
    memory.WriteU32(OldNode, OldData);
    memory.WriteU32(OldNode + 4u, 0u);
    for (unsigned i = 0; i < 4u; ++i)
    {
        memory.WriteU8(Source + i,
            static_cast<std::uint8_t>("DATA"[i]));
        memory.WriteU8(OldData + i,
            static_cast<std::uint8_t>("OLD!"[i]));
    }
}

PPCContext Initial(Route route)
{
    auto context = close_shared_oracle::Initial(
        close_shared_oracle::Route::NullPath);
    context.r1.u64 = 0x8877665500000000ull | Stack;
    context.lr = 0x1234567887654321ull;
    context.r3.u64 = 0xaabbccdd00000000ull | Reader;
    if (route == Route::NullName)
        context.r4.u64 = 0u;
    else
    {
        context.r4.u64 = 4u;
        context.r5.u64 = Source;
        context.r6.u64 = 0u;
        context.r7.u64 = 3u;
    }
    return context;
}

void Check(const Case& item)
{
    GuestWindow original(ReaderRegions), recovered(ReaderRegions);
    Seed(original);
    Seed(recovered);
    Services expected(original, Mode::LockedWrite);
    Services actual(recovered, Mode::LockedWrite);
    IndexService expected_index, actual_index;
    Host expected_host(Scenario::BinarySuccess);
    Host actual_host(Scenario::BinarySuccess);
    wrapper_oracle::WrapperUnlock expected_unlock, actual_unlock;
    close_shared_oracle::SharedExtra expected_extra, actual_extra;
    Guest expected_guest(item.route), actual_guest(item.route);
    auto context = Initial(item.route);
    auto state = crt_full_oracle::FromPpc(context);
    current = &expected;
    active = &expected;
    current_index = &expected_index;
    current_host = &expected_host;
    wrapper_oracle::original_unlock = &expected_unlock;
    close_shared_oracle::active_extra = &expected_extra;
    original_extra = &expected_extra;
    original_guest = &expected_guest;
    if (item.entry == 0x82bd0a60u)
        __imp__sub_82BD0A60(context, original.Bytes());
    else __imp__sub_82BD0D28(context, original.Bytes());
    current = nullptr;
    active = nullptr;
    current_index = nullptr;
    current_host = nullptr;
    wrapper_oracle::original_unlock = nullptr;
    close_shared_oracle::active_extra = nullptr;
    original_extra = nullptr;
    original_guest = nullptr;
    if (!family::Apply(item.entry, actual.memory,
            Deps(actual, actual_index, actual_host, actual_unlock,
                actual_extra, actual_guest), state))
        throw std::runtime_error("missing selected reader caller");
    const auto before = crt_full_oracle::Snapshot(
        crt_full_oracle::FromPpc(context));
    const auto after = crt_full_oracle::Snapshot(state);
    if (before != after || !original.EqualCommitted(recovered) ||
        expected.events != actual.events ||
        expected.traps != actual.traps ||
        expected_index.events != actual_index.events ||
        expected_host.events != actual_host.events ||
        expected_unlock.events != actual_unlock.events ||
        expected_extra.events != actual_extra.events ||
        expected_guest.events != actual_guest.events)
    {
        for (unsigned i = 0; i < before.size(); ++i)
            if (before[i] != after[i])
                std::fprintf(stderr, "%s state[%u] %llx/%llx\n", item.name,
                    i, static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("reader caller full state/RAM/callback mismatch");
    }
    const auto memory = expected.memory;
    if (item.route == Route::CopySuccess &&
        (expected_guest.events.size() != 2u ||
            memory.ReadU32(Reader) != NewNode ||
            memory.ReadU32(Reader + 4u) != NewNode ||
            memory.ReadU32(Reader + 16u) != NewData ||
            memory.ReadU32(NewNode + 4u) != 3u ||
            memory.ReadU8(NewData) != 'D' ||
            memory.ReadU8(NewData + 3u) != 'A'))
        throw std::runtime_error("allocated copy path absent");
    if (item.route == Route::SecondAllocationFailure &&
        (expected_guest.events.size() != 2u ||
            memory.ReadU32(Reader) != OldNode ||
            memory.ReadU32(Reader + 16u) != OldData ||
            memory.ReadU32(OldNode + 4u) != 3u ||
            memory.ReadU8(OldData) != 'D'))
        throw std::runtime_error("second allocation fallback absent");
    if (item.route == Route::NullName &&
        (context.r3.u32 != Reader ||
            memory.ReadU32(Reader + 8u) != 0u ||
            memory.ReadU8(Reader + 24u) != 0u ||
            !expected_guest.events.empty()))
        throw std::runtime_error("null-name caller path absent");
}
} // namespace reader_callers_oracle

void OriginalReaderSave(unsigned first, PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned i = first; i <= 31u; ++i)
        lo::semantic::gpu::recovery_abi::WriteU64(current->memory,
            lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - i)), gprs[i]->u64);
    current->memory.WriteU32(lo::semantic::gpu::recovery_abi::Address(
        context.r1.u64 - 8u), context.r12.u32);
}

void OriginalReaderRestore(unsigned first, PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned i = first; i <= 31u; ++i)
        gprs[i]->u64 = lo::semantic::gpu::recovery_abi::ReadU64(
            current->memory, lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - i)));
    context.r12.u64 = current->memory.ReadU32(
        lo::semantic::gpu::recovery_abi::Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}

void OriginalReaderIndirect(GuestAddress target, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    reader_callers_oracle::original_guest->CallIndirect(target,
        current->memory, state);
    crt_full_oracle::ToPpc(context, state);
}

void OriginalReaderDirect(GuestAddress entry, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    const auto deps = reader_callers_oracle::Deps(*current, *current_index,
        *current_host, *wrapper_oracle::original_unlock,
        *reader_callers_oracle::original_extra,
        *reader_callers_oracle::original_guest);
    if (entry == 0x82bd0798u)
    {
        if (!crt_close_recursive_buffer_context::Apply(entry,
                current->memory, deps.guest, state))
            throw std::runtime_error("missing lazy table lower");
    }
    else if (entry == 0x82df2520u)
    {
        if (!crt_stream_block_read_context::Apply(entry,
                current->memory, deps.accepted, state))
            throw std::runtime_error("missing block read lower");
    }
    else if (entry == 0x82b7a0b0u)
    {
        if (!crt_copy_full_context::Apply(entry,current->memory,state))
            throw std::runtime_error("missing full copy lower");
    }
    else if (entry == 0x82df1ea8u)
    {
        if (!crt_stream_close_reopen_context::Apply(entry,
                current->memory, deps.accepted, state))
            throw std::runtime_error("missing reopen lower");
    }
    else if (entry == 0x82df1cd0u)
    {
        if (!crt_stream_close_file_lower::Apply(entry,
                current->memory, deps.accepted, state))
            throw std::runtime_error("missing file lower");
    }
    else if (entry == 0x82b85d88u)
    {
        auto lower = crt_context_adapter::ToStream(state);
        if (!crt_stream_bulk_close_routes::Apply(entry,
                current->memory, deps.accepted.close, lower))
            throw std::runtime_error("missing bulk close lower");
        crt_context_adapter::FromStream(state, lower);
    }
    else throw std::runtime_error("unknown reader lower");
    crt_full_oracle::ToPpc(context,state);
}

int main()
{
    for (const auto& item : reader_callers_oracle::Cases)
    {
        try { reader_callers_oracle::Check(item); }
        catch (const std::exception& error)
        { std::fprintf(stderr, "%s: %s\n", item.name, error.what()); return 1; }
    }
    std::puts("PASS crt-close-reader-callers-context 3 focused actual PPC cases");
    std::puts("LIMIT accepted lower selected ABI and mutable guest allocator; fault/MMIO/concurrency/runtime unvalidated");
    return 0;
}
