// Appended after the pinned PPC bodies by semantic_recovery.py.
#include "lo_semantics/pending_record_cleanup.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = pending_record_cleanup;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Record = 0x30000u;
constexpr GuestAddress Target = 0x40000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<Region, 1> Regions{{{0u, 0x90000u}}};

enum class Mode { Idle, Overflow, Alias, Funclet };
struct Case { GuestAddress entry; Mode mode; };
constexpr std::array<Case, 4> Cases{{
    {0x82373158u, Mode::Idle},
    {0x82373158u, Mode::Overflow},
    {0x82373158u, Mode::Alias},
    {0x82290640u, Mode::Funclet},
}};

struct Services final : family::NativeServices
{
    unsigned sync_calls = 0;
    void LightweightSync() override { ++sync_calls; }
};

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
    state.cr0 = {std::uint8_t(context.cr0.lt),
        std::uint8_t(context.cr0.gt), std::uint8_t(context.cr0.eq),
        std::uint8_t(context.cr0.so)};
    state.cr6 = {std::uint8_t(context.cr6.lt),
        std::uint8_t(context.cr6.gt), std::uint8_t(context.cr6.eq),
        std::uint8_t(context.cr6.so)};
    return state;
}

bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr && a.ctr == b.ctr &&
        a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so;
}

void Seed(GuestWindow& window, Mode mode)
{
    window.Fill(0);
    auto memory = window.Memory();
    memory.WriteU32(Record, mode == Mode::Alias ? Record : Target);
    memory.WriteU32(Record + 4u, mode == Mode::Idle ? 0u : 1u);
    memory.WriteU32(Record + 8u,
        mode == Mode::Overflow ? 0xffffffffu :
        mode == Mode::Alias ? 5u : 7u);
    memory.WriteU32(Record + 16u, 0x77777777u);
    memory.WriteU32(Target + 8u,
        mode == Mode::Overflow ? 0xffffffffu : 9u);
    memory.WriteU32(Target + 16u, 0x55555555u);
}

PPCContext Initial(Mode mode)
{
    PPCContext context{};
    PPCRegister* fields[] = {&context.r0, &context.r1, &context.r2,
        &context.r3, &context.r4, &context.r5, &context.r6, &context.r7,
        &context.r8, &context.r9, &context.r10, &context.r11, &context.r12,
        &context.r13, &context.r14, &context.r15, &context.r16, &context.r17,
        &context.r18, &context.r19, &context.r20, &context.r21, &context.r22,
        &context.r23, &context.r24, &context.r25, &context.r26, &context.r27,
        &context.r28, &context.r29, &context.r30, &context.r31};
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    context.r1.u64 = 0x1234567800000000ull | Stack;
    context.lr = 0x11223344abcdef01ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.r3.u64 = Record;
    if (mode == Mode::Funclet)
        context.r12.u64 = 0x1234567800000000ull | (Record + 408u);
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    return context;
}

bool ExpectedPath(Mode mode, const PPCContext& context,
    const GuestWindow& window)
{
    const auto memory = window.Memory();
    if (mode == Mode::Idle)
        return context.r11.u64 == 0u && context.cr6.eq &&
            memory.ReadU32(Record + 4u) == 0u &&
            memory.ReadU32(Record + 16u) == 0x77777777u &&
            memory.ReadU32(Target + 16u) == 0x55555555u;

    const auto expected_sum = mode == Mode::Overflow ?
        0x1fffffffeull : mode == Mode::Alias ? 10ull : 16ull;
    const auto actual_target = mode == Mode::Alias ? Record : Target;
    if (context.r10.u64 != expected_sum || context.r11.u64 != actual_target ||
        memory.ReadU32(actual_target + 8u) !=
            static_cast<std::uint32_t>(expected_sum) ||
        memory.ReadU32(actual_target + 16u) != 0u ||
        memory.ReadU32(Record + 4u) != 0u)
        return false;

    if (mode == Mode::Funclet)
        return context.r3.u64 == (0x1234567800000000ull | Record) &&
            context.r31.u64 ==
                (0x1234567800000000ull | (Record - 104u)) &&
            context.r1.u64 == (0x1234567800000000ull | Stack) &&
            context.lr == 0xabcdef01u;
    return true;
}

bool Check(const Case& item)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, item.mode);
    Seed(recovered, item.mode);
    Services native;
    auto raw = Initial(item.mode);
    auto state = FromPpc(raw);
    std::fprintf(stderr, "pending original entry=%08X mode=%u\n",
        item.entry, static_cast<unsigned>(item.mode));
    if (item.entry == 0x82373158u)
        __imp__sub_82373158(raw, original.Bytes());
    else
        __imp__sub_82290640(raw, original.Bytes());
    if (!ExpectedPath(item.mode, raw, original))
        throw std::runtime_error("pending fixture missed original path");
    auto recovered_memory = recovered.Memory();
    if (!family::Apply(item.entry, recovered_memory, native, state))
        throw std::runtime_error("pending recovered entry missing");

    const auto observed = FromPpc(raw);
    const bool ram_equal = original.EqualCommitted(recovered);
    const unsigned expected_sync = item.mode == Mode::Idle ? 0u : 1u;
    if (!Same(observed, state) || !ram_equal ||
        native.sync_calls != expected_sync)
    {
        std::fprintf(stderr,
            "FAIL pending %08X mode=%u r3=%llX/%llX sp=%llX/%llX "
            "lr=%llX/%llX ctr=%llX/%llX RAM=%u sync=%u/%u "
            "CR6=%u%u%u%u/%u%u%u%u CA=%u/%u\n",
            item.entry, static_cast<unsigned>(item.mode),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr), ram_equal,
            native.sync_calls, expected_sync,
            observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
            observed.cr6.so, state.cr6.lt, state.cr6.gt, state.cr6.eq,
            state.cr6.so, observed.xer_ca, state.xer_ca);
        for (unsigned index = 0; index < 32u; ++index)
            if (observed.r[index] != state.r[index])
                std::fprintf(stderr, " r%u=%llX/%llX", index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        std::fprintf(stderr, "\n");
        return false;
    }
    return true;
}
} // namespace

int main()
{
    try
    {
        for (const auto& item : Cases)
            if (!Check(item)) return 1;
        std::printf("PASS pending-record-cleanup %zu focused original PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected ABI/RAM only; lwsync ordering, concurrency, faults and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
