#include "lo_semantics/legacy_descriptor_search_helpers.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_search_helpers;
constexpr GuestAddress A = 0x10000u, B = 0x20000u, Link = 0x30000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x90000u}}};
enum class Mode { Type21, Type50, FeatureRecord, FeatureOther,
    Normalize, OverlapGreater, OverlapTaggedEnd, OverlapFallback,
    WalkTwoLinks, WalkNull };
struct Case
{
    const char* name;
    GuestAddress entry;
    Mode mode;
    std::uint32_t expected;
};
constexpr std::array<Case, 10> Cases{{
    {"ordinal-type-21", 0x82fac070u, Mode::Type21, 1u},
    {"ordinal-type-50", 0x82fac070u, Mode::Type50, 0u},
    {"feature-record-124", 0x82fac608u, Mode::FeatureRecord, 0u},
    {"feature-other-50", 0x82fac608u, Mode::FeatureOther, 1u},
    {"normalize-four-pairs", 0x83054600u, Mode::Normalize, 0x88u},
    {"overlap-rank-greater", 0x82fb71d0u, Mode::OverlapGreater, 1u},
    {"overlap-tagged-end", 0x82fb71d0u, Mode::OverlapTaggedEnd, 0u},
    {"overlap-fallback-link", 0x82fb71d0u, Mode::OverlapFallback, 1u},
    {"walk-two-links", 0x83057b58u, Mode::WalkTwoLinks, 1u},
    {"walk-null", 0x83057b58u, Mode::WalkNull, 0u}
}};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers s{};
    const auto fields = Fields(c);
    for (unsigned i = 0u; i < 32u; ++i)
        s.r[i] = i == 1u ? 0u : fields[i]->u64;
    s.sp = c.r1.u64; s.lr = c.lr; s.ctr = c.ctr.u64;
    s.xer_so = c.xer.so; s.xer_ca = c.xer.ca;
    s.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    return s;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }
bool Same(const family::Registers& a, const family::Registers& b)
{
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr && a.ctr == b.ctr &&
        a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        SameCondition(a.cr0, b.cr0) && SameCondition(a.cr6, b.cr6);
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned i = 0u; i < 32u; ++i)
        fields[i]->u64 = 0x1122334400000000ull + i;
    c.r1.u64 = 0x1234567800080000ull;
    c.r3.u64 = 0x1234567800000000ull |
        ((item.mode == Mode::WalkTwoLinks || item.mode == Mode::WalkNull)
            ? B : (item.mode == Mode::Normalize ? 0xe4u : A));
    c.r4.u64 = 0x5566778800000000ull |
        ((item.mode == Mode::WalkTwoLinks || item.mode == Mode::WalkNull)
            ? A : B);
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    return c;
}
void Seed(GuestMemory& m, const Case& item)
{
    switch (item.mode)
    {
    case Mode::Type21: m.WriteU32(A + 8u, 21u << 7u); break;
    case Mode::Type50: m.WriteU32(A + 8u, 50u << 7u); break;
    case Mode::FeatureRecord:
        m.WriteU32(A, 0x02000000u); m.WriteU32(A + 12u, B);
        m.WriteU32(B + 8u, 124u << 7u); break;
    case Mode::FeatureOther:
        m.WriteU32(A, 0x02000000u); m.WriteU32(A + 12u, B);
        m.WriteU32(B + 8u, 50u << 7u); break;
    case Mode::OverlapGreater:
        m.WriteU8(A + 12u, 1u); m.WriteU8(B + 12u, 1u);
        m.WriteU32(A + 16u, 10u); m.WriteU32(B + 16u, 5u); break;
    case Mode::OverlapTaggedEnd:
        m.WriteU8(A + 12u, 1u); m.WriteU8(B + 12u, 1u);
        m.WriteU32(A + 16u, 5u); m.WriteU32(B + 16u, 10u);
        m.WriteU32(A + 36u, 1u); break;
    case Mode::OverlapFallback:
        m.WriteU8(A + 12u, 0u); m.WriteU8(B + 12u, 1u);
        m.WriteU32(B + 32u, A + 36u); break;
    case Mode::WalkTwoLinks:
        m.WriteU32(B + 32u, Link + 36u);
        m.WriteU32(Link + 32u, A + 36u); break;
    case Mode::WalkNull: m.WriteU32(B + 32u, 36u); break;
    case Mode::Normalize: break;
    }
}
void Original(const Case& item, PPCContext& c, std::uint8_t* base)
{
    switch (item.entry)
    {
    case 0x82fac070u: __imp__sub_82FAC070(c, base); return;
    case 0x82fac608u: __imp__sub_82FAC608(c, base); return;
    case 0x83054600u: __imp__sub_83054600(c, base); return;
    case 0x82fb71d0u: __imp__sub_82FB71D0(c, base); return;
    case 0x83057b58u: __imp__sub_83057B58(c, base); return;
    default: throw std::runtime_error("unknown original entry");
    }
}
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item); auto state = FromPpc(context);
    Original(item, context, original.Bytes());
    if (!family::Apply(item.entry, right, state))
        throw std::runtime_error("semantic search helper entry missing");
    const auto observed = FromPpc(context);
    if (static_cast<std::uint32_t>(context.r3.u64) != item.expected ||
        static_cast<std::uint32_t>(state.r[3]) != item.expected ||
        !Same(observed, state) || !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,
            "mismatch %s entry=%08X r3=%016llX/%016llX expected=%08X SP=%016llX/%016llX LR=%016llX/%016llX CTR=%016llX/%016llX CA=%u/%u CR0=%u,%u,%u/%u,%u,%u CR6=%u,%u,%u/%u,%u,%u\n",
            item.name, item.entry,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]), item.expected,
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            static_cast<unsigned long long>(observed.ctr),
            static_cast<unsigned long long>(state.ctr),
            observed.xer_ca, state.xer_ca,
            observed.cr0.lt, observed.cr0.gt, observed.cr0.eq,
            state.cr0.lt, state.cr0.gt, state.cr0.eq,
            observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
            state.cr6.lt, state.cr6.gt, state.cr6.eq);
        for (unsigned i = 0; i < 32u; ++i)
            if (observed.r[i] != state.r[i])
                std::fprintf(stderr, "r%u=%016llX/%016llX\n", i,
                    static_cast<unsigned long long>(observed.r[i]),
                    static_cast<unsigned long long>(state.r[i]));
        for (const auto region : Regions)
            for (std::size_t offset = 0; offset < region.size; ++offset)
            {
                const auto address = std::size_t(region.base) + offset;
                if (original.Bytes()[address] != recovered.Bytes()[address])
                {
                    std::fprintf(stderr, "RAM first %08zX=%02X/%02X\n",
                        address, original.Bytes()[address],
                        recovered.Bytes()[address]);
                    throw std::runtime_error(item.name);
                }
            }
        throw std::runtime_error(item.name);
    }
}
} // namespace

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-search-helpers %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size());
        std::puts("LIMIT parent 83058FC8 and runtime memory topology remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
