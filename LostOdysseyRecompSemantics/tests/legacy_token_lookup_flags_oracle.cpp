// Appended after the two complete original PPC bodies by the runner.
#include "lo_semantics/legacy_token_lookup_flags.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_token_lookup_flags;
using test::GuestWindow;
using test::Region;

constexpr GuestAddress Table = 0x20000u;
constexpr GuestAddress Buckets = 0x30000u;
constexpr GuestAddress Nodes = 0x40000u;
constexpr GuestAddress Key = 0x50000u;
constexpr GuestAddress Flags = 0x60000u;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress CharacterTablePointer = 0x83215b40u;
constexpr std::uint32_t KeyHigh = 0x12345679u;
constexpr std::uint32_t KeyLow = 0xabcdef03u;
constexpr std::uint32_t FoundValue = 0x76543210u;
constexpr std::array<Region, 2> Regions{{{0u, 0x90000u},
    {0x83214000u, 0x2000u}}};

enum class Mode { NoBuckets, ZeroCount, EmptyBucket, DirectHit,
    CollisionHit, ChainMiss, FlagHit, Flag255, Flag256, FlagSentinel };
constexpr std::array<Mode, 10> Cases{{Mode::NoBuckets,
    Mode::ZeroCount, Mode::EmptyBucket, Mode::DirectHit,
    Mode::CollisionHit, Mode::ChainMiss, Mode::FlagHit,
    Mode::Flag255, Mode::Flag256, Mode::FlagSentinel}};

bool IsFlag(Mode mode)
{
    return mode == Mode::FlagHit || mode == Mode::Flag255 ||
        mode == Mode::Flag256 || mode == Mode::FlagSentinel;
}

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
    memory.WriteU32(CharacterTablePointer, Flags);
    memory.WriteU16(Flags + 2u * 65u, 0x36u);
    memory.WriteU16(Flags + 2u * 255u, 0x8001u);
    if (IsFlag(mode)) return;

    memory.WriteU32(Table, Nodes);
    memory.WriteU32(Table + 4u, mode == Mode::ZeroCount ? 0u : 2u);
    memory.WriteU32(Table + 12u, mode == Mode::NoBuckets ? 0u : Buckets);
    memory.WriteU32(Table + 16u, 4u);
    memory.WriteU32(Key, KeyHigh);
    memory.WriteU32(Key + 4u, KeyLow);
    memory.WriteU32(Buckets + 4u,
        mode == Mode::EmptyBucket ? 0xffffffffu :
        mode == Mode::DirectHit ? 1u : 0u);
    memory.WriteU32(Nodes, 1u);
    memory.WriteU32(Nodes + 4u,
        mode == Mode::ChainMiss ? KeyHigh : KeyHigh - 1u);
    memory.WriteU32(Nodes + 8u, KeyLow + 1u);
    memory.WriteU32(Nodes + 12u, 0x11111111u);
    memory.WriteU32(Nodes + 16u, 0xffffffffu);
    memory.WriteU32(Nodes + 20u,
        mode == Mode::ChainMiss ? KeyHigh - 1u : KeyHigh);
    memory.WriteU32(Nodes + 24u, KeyLow);
    memory.WriteU32(Nodes + 28u, FoundValue);
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
    context.lr = 0xabcdef0123456789ull;
    context.ctr.u64 = 0x5555666677778888ull;
    context.cr0.gt = 1;
    context.cr6.lt = 1;
    context.xer.so = 1;
    context.xer.ca = 1;
    if (IsFlag(mode))
    {
        const std::uint32_t code = mode == Mode::Flag255 ? 255u :
            mode == Mode::Flag256 ? 256u :
            mode == Mode::FlagSentinel ? 65535u : 65u;
        const std::uint32_t mask = mode == Mode::Flag255 ? 0x8000u : 0x12u;
        context.r3.u64 = 0x1234567800000000ull | code;
        context.r4.u64 = 0x8765432100000000ull | mask;
    }
    else
    {
        context.r3.u64 = 0x1234567800000000ull | Table;
        context.r4.u64 = 0x8765432100000000ull | Key;
    }
    return context;
}

bool ExpectedPath(Mode mode, const PPCContext& context)
{
    std::uint32_t result = 0u;
    if (mode == Mode::DirectHit || mode == Mode::CollisionHit)
        result = FoundValue;
    else if (mode == Mode::FlagHit) result = 0x12u;
    else if (mode == Mode::Flag255) result = 0x8000u;
    return context.r3.u64 == result;
}

bool Check(Mode mode)
{
    GuestWindow original(Regions), recovered(Regions);
    Seed(original, mode);
    Seed(recovered, mode);
    auto raw = Initial(mode);
    auto state = FromPpc(raw);
    const GuestAddress entry = IsFlag(mode) ? 0x822974b0u : 0x822972a8u;
    if (IsFlag(mode)) __imp__sub_822974B0(raw, original.Bytes());
    else __imp__sub_822972A8(raw, original.Bytes());
    if (!ExpectedPath(mode, raw))
        throw std::runtime_error("lookup/flags fixture missed original path");
    auto memory = recovered.Memory();
    if (!family::Apply(entry, memory, state))
        throw std::runtime_error("lookup/flags recovered entry missing");
    const auto observed = FromPpc(raw);
    const bool ram_equal = original.EqualCommitted(recovered);
    if (!Same(observed, state) || !ram_equal)
    {
        std::fprintf(stderr, "FAIL lookup/flags mode=%u r3=%llX/%llX "
            "RAM=%u CR6=%u%u%u%u/%u%u%u%u CA=%u/%u\n",
            static_cast<unsigned>(mode),
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]), ram_equal,
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
        for (const auto mode : Cases)
            if (!Check(mode)) return 1;
        std::printf("PASS legacy-token-lookup-flags %zu original PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected ABI/RAM; other inputs, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
