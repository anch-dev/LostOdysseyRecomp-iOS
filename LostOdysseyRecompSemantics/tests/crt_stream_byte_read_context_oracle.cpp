#define main ByteReadSharedFixtureMain
#include "crt_stream_close_shared_lower_oracle.cpp"
#undef main

#include "lo_semantics/crt_context_adapter.h"
#include "lo_semantics/crt_stream_byte_read_context.h"

namespace byte_read_oracle
{
namespace family = crt_stream_byte_read_context;
using Full = family::Registers;
enum class Route { NullStream, Disallowed, RefillError, BufferedSuccess };
struct Case { const char* name; Route route; };
constexpr std::array Cases{
    Case{"null-stream", Route::NullStream},
    Case{"read-disallowed", Route::Disallowed},
    Case{"refill-invalid-handle", Route::RefillError},
    Case{"buffered-byte-success", Route::BufferedSuccess}};

using NativeEvent = std::array<std::uint64_t, 8>;
struct ReadNative final : crt_async_status_transfer::NativeServices,
    crt_utf8_conversion_routes::NativeServices,
    heap_allocation_context::BoundaryServices,
    crt_free_context::LowerCalls
{
    std::vector<NativeEvent> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory,
        Full& state) override
    {
        if (target != 0x2400u || state.lr != 0x82be2ed8u)
            throw std::runtime_error("unexpected byte-read native target");
        events.push_back({target, state.r[1], state.lr, state.r[3],
            state.r[4], state.r[5], state.r[8], state.r[9]});
        for (unsigned i = 0; i < 4u; ++i)
            memory.WriteU8(Address(state.r[8] + i),
                static_cast<std::uint8_t>("ABCD"[i]));
        memory.WriteU32(Address(state.r[1] + 84u), 4u);
        state.r[3] = 0u;
        state.r[10] = 0x1122334455667788ull;
        state.fpr_bits[3] = 0x4008000000000000ull;
        state.cr1.gt ^= 1u;
        state.cr7.eq ^= 1u;
    }
    void NtWaitForSingleObjectEx(GuestMemory&, Full&) override
    { throw std::runtime_error("unexpected async wait"); }
    void NtStatusToDosError(GuestMemory&, Full&) override
    { throw std::runtime_error("unexpected status conversion"); }
    void RtlMultiByteToUnicodeN(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override
    { throw std::runtime_error("unexpected conversion"); }
    void RtlNtStatusToDosError(GuestMemory&,
        crt_utf8_conversion_routes::Registers&) override
    { throw std::runtime_error("unexpected conversion status"); }
    void CallDirect(GuestAddress, GuestMemory&,
        heap_allocation_context::Registers&) override
    { throw std::runtime_error("unexpected heap guest call"); }
    void CallNative(GuestAddress, GuestMemory&,
        heap_allocation_context::Registers&) override
    { throw std::runtime_error("unexpected heap native call"); }
    void Call(GuestAddress, GuestMemory&,
        crt_free_context::Registers&) override
    { throw std::runtime_error("unexpected free lower call"); }
};

close_shared_oracle::SharedExtra* original_extra = nullptr;
ReadNative* original_native = nullptr;

family::Dependencies Deps(Services& stream, IndexService& index,
    Host& host, wrapper_oracle::WrapperUnlock& unlock,
    close_shared_oracle::SharedExtra& extra, ReadNative& native)
{
    auto shared = close_shared_oracle::Deps(stream, index, host,
        unlock, extra);
    const auto& open = shared.open.open;
    crt_stream_open_pipeline::Dependencies pipeline{
        open.stream, open.open, open.position,
        {open.read.stream, native, native, native, native},
        open.close, open.failure};
    return {shared.close, {pipeline, shared.open.unlock}, shared.native};
}

void Seed(GuestWindow& window, Route route)
{
    close_shared_oracle::SeedShared(window,
        close_shared_oracle::Route::NullPath);
    auto memory = window.Memory();
    std::uint32_t flags = 0u;
    if (route == Route::Disallowed) flags = 2u;
    if (route == Route::RefillError || route == Route::BufferedSuccess)
        flags = 9u;
    memory.WriteU32(Stream + 12u, flags);
    memory.WriteU32(Stream + 8u, Buffer);
    memory.WriteU32(Stream + 16u,
        route == Route::BufferedSuccess ? 5u : 0xfffffffeu);
    memory.WriteU32(Stream + 24u,
        route == Route::BufferedSuccess ? 4u : 2u);
    memory.WriteU32(Stream, Buffer);
    memory.WriteU32(Stream + 4u, 1u);
    memory.WriteU32(0x34010u, 0x2401u);
}

PPCContext Initial(Route route)
{
    auto context = close_shared_oracle::Initial(
        close_shared_oracle::Route::NullPath);
    context.r3.u64 = route == Route::NullStream ? 0u :
        (0x8877665500000000ull | Stream);
    context.r1.u64 = 0x8877665500000000ull | Stack;
    context.lr = 0x1234567887654321ull;
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
    close_shared_oracle::SharedExtra expected_extra, actual_extra;
    ReadNative expected_native, actual_native;
    auto context = Initial(item.route);
    auto state = crt_full_oracle::FromPpc(context);
    current = &expected;
    active = &expected;
    current_index = &expected_index;
    current_host = &expected_host;
    wrapper_oracle::original_unlock = &expected_unlock;
    close_shared_oracle::active_extra = &expected_extra;
    original_extra = &expected_extra;
    original_native = &expected_native;
    __imp__sub_82B81360(context, original.Bytes());
    current = nullptr;
    active = nullptr;
    current_index = nullptr;
    current_host = nullptr;
    wrapper_oracle::original_unlock = nullptr;
    close_shared_oracle::active_extra = nullptr;
    original_extra = nullptr;
    original_native = nullptr;
    if (!family::Apply(0x82b81360u, actual.memory,
        Deps(actual, actual_index, actual_host, actual_unlock,
            actual_extra, actual_native), state))
        throw std::runtime_error("missing selected byte-read body");
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
        expected_native.events != actual_native.events)
    {
        for (unsigned i = 0; i < before.size(); ++i)
            if (before[i] != after[i])
                std::fprintf(stderr, "%s state[%u] %llx/%llx\n", item.name,
                    i, static_cast<unsigned long long>(before[i]),
                    static_cast<unsigned long long>(after[i]));
        throw std::runtime_error("byte-read full state/RAM/callback mismatch");
    }
    const auto memory = expected.memory;
    if (item.route != Route::BufferedSuccess && context.r3.s32 != -1)
        throw std::runtime_error("byte-read failure result absent");
    if (item.route == Route::NullStream &&
        memory.ReadU32(0x83215210u) != 22u)
        throw std::runtime_error("null stream did not set errno 22");
    if (item.route == Route::Disallowed &&
        memory.ReadU32(Stream + 12u) != 34u)
        throw std::runtime_error("read-disallowed flag absent");
    if (item.route == Route::RefillError &&
        (memory.ReadU32(Stream + 4u) != 0u ||
            memory.ReadU32(Stream + 12u) != 41u ||
            memory.ReadU32(0x83215210u) != 9u))
        throw std::runtime_error("refill error path absent");
    if (item.route == Route::BufferedSuccess &&
        (context.r3.u32 != static_cast<std::uint32_t>('A') ||
            memory.ReadU32(Stream + 4u) != 3u ||
            memory.ReadU32(Stream) != Buffer + 1u ||
            memory.ReadU8(Buffer) != 'A' ||
            expected_native.events.size() != 1u))
        throw std::runtime_error("buffered byte success path absent");
}
} // namespace byte_read_oracle

void OriginalByteReadSave(PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned i = 29u; i <= 31u; ++i)
        lo::semantic::gpu::recovery_abi::WriteU64(current->memory,
            lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - i)), gprs[i]->u64);
    current->memory.WriteU32(lo::semantic::gpu::recovery_abi::Address(
        context.r1.u64 - 8u), context.r12.u32);
}

void OriginalByteReadRestore(PPCContext& context)
{
    const auto gprs = crt_full_oracle::Gprs(context);
    for (unsigned i = 29u; i <= 31u; ++i)
        gprs[i]->u64 = lo::semantic::gpu::recovery_abi::ReadU64(
            current->memory, lo::semantic::gpu::recovery_abi::Address(
                context.r1.u64 - 8u * (33u - i)));
    context.r12.u64 = current->memory.ReadU32(
        lo::semantic::gpu::recovery_abi::Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}

void OriginalByteReadLower(GuestAddress entry, PPCContext& context,
    std::uint8_t*)
{
    auto state = crt_full_oracle::FromPpc(context);
    const auto dependencies = byte_read_oracle::Deps(*current, *current_index,
        *current_host, *wrapper_oracle::original_unlock,
        *byte_read_oracle::original_extra,
        *byte_read_oracle::original_native);
    if (entry == 0x82b85a80u)
    {
        if (!crt_stream_refill_context::Apply(entry, current->memory,
                dependencies, state))
            throw std::runtime_error("missing selected refill lower");
    }
    else if (entry == 0x82b7fd78u || entry == 0x82b7fec0u)
    {
        if (!crt_stream_close_shared_lower::ApplyAcceptedLower(entry,
                current->memory, dependencies, state))
            throw std::runtime_error("missing accepted error lower");
    }
    else
    {
        auto lower = crt_context_adapter::ToStream(state);
        if (!crt_stream_operations::ApplyAcceptedCallee(entry,
                current->memory,
                dependencies.close.pipeline.close.accepted, lower))
            throw std::runtime_error("missing accepted state lower");
        crt_context_adapter::FromStream(state, lower);
    }
    crt_full_oracle::ToPpc(context, state);
}

int main()
{
    for (const auto& item : byte_read_oracle::Cases)
    {
        try { byte_read_oracle::Check(item); }
        catch (const std::exception& error)
        { std::fprintf(stderr, "%s: %s\n", item.name, error.what()); return 1; }
    }
    std::puts("PASS crt-stream-byte-read-context 4 focused actual PPC cases");
    std::puts("LIMIT selected accepted stream state/error and refill lower ABI; fault/MMIO/concurrency/runtime unvalidated");
    return 0;
}
