#include "lo_semantics/legacy_descriptor_search_caller.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_search_caller;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Output = 0x20000u;
constexpr GuestAddress Descriptor = 0x30000u;
constexpr GuestAddress ChildType = 0x40000u;
constexpr GuestAddress Aux = 0x50000u;
constexpr GuestAddress Node = 0x60000u;
constexpr GuestAddress Global = 0x70000u;
constexpr GuestAddress Other = 0x71000u;
constexpr GuestAddress List = 0x72000u;
constexpr GuestAddress Entry = 0x73000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x90000u}}};
struct Case
{
    const char* name;
    std::uint32_t type, bits;
    std::uint32_t mode;
    bool layout, feature, overlap, diagnostic;
};
constexpr std::array<Case, 8> Cases{{
    {"small-type-plain", 1u, 0u, 0u, false, false, false, false},
    {"small-type-ordinal", 21u, 0u, 0u, false, false, false, false},
    {"small-type-layout", 30u, 0x00080000u, 0u, true, false, false, false},
    {"large-type-plain", 31u, 0u, 0u, false, false, false, false},
    {"large-type-active", 31u, 0u, 1u, false, false, false, false},
    {"large-type-feature-layout", 107u, 0x00080000u, 1u,
        true, true, false, false},
    {"small-type-diagnostic", 1u, 0u, 0u, false, false, false, true},
    {"large-type-overlap", 31u, 0x20u, 0u,
        false, false, true, false}
}};
unsigned ordinal_calls = 0, feature_calls = 0, normalize_calls = 0;
unsigned overlap_calls = 0, walk_calls = 0, layout_calls = 0;
unsigned classifier_calls = 0, selector_calls = 0;
unsigned diagnostic_calls = 0, diagnostic_lower_calls = 0;

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
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1234567800000000ull | Object;
    c.r4.u64 = 0x5566778800000000ull | Output;
    c.r5.u64 = 0x99aabbcc00000000ull | item.mode;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    return c;
}
void Seed(GuestMemory& m, const Case& item)
{
    m.WriteU32(0u, Global);
    m.WriteU32(Global + 148u, Aux);
    m.WriteU32(Object, 0u);
    m.WriteU32(Object + 4u, 0u);
    m.WriteU32(Object + 8u, (item.type << 7u) | item.bits);
    m.WriteU8(Object + 12u, 1u);
    m.WriteU32(Object + 16u, 1u);
    m.WriteU32(Object + 24u, Aux);
    m.WriteU32(Object + 32u, Node + 36u);
    m.WriteU32(Object + 40u, Descriptor);
    m.WriteU32(Object + 44u, Descriptor);
    m.WriteU32(Object + 48u, Descriptor);
    m.WriteU32(Aux + 76u, 0u);
    m.WriteU32(Node + 8u, 0u);
    m.WriteU32(Descriptor, (1u << 25u) | (0x39u << 5u) | 0x14u);
    m.WriteU32(Descriptor + 12u, ChildType);
    m.WriteU32(ChildType + 8u, 50u << 7u);
    m.WriteU32(Output, 0u);
    m.WriteU32(Output + 4u, item.diagnostic ? 0x20000000u : 0u);
    m.WriteU32(Output + 8u, 0u);
    if (item.overlap)
    {
        m.WriteU32(Object + 4u, Node);
        m.WriteU32(Node, 0x02000000u);
        m.WriteU32(Node + 8u, 0u);
        m.WriteU32(Node + 16u, List);
        m.WriteU32(List, Entry);
        m.WriteU32(Entry, 0x02000000u);
        m.WriteU32(Entry + 4u, 0u);
        m.WriteU32(Entry + 12u, Other);
        m.WriteU32(Other + 8u, (31u << 7u) | 0x20u);
        m.WriteU8(Other + 12u, 1u);
        m.WriteU32(Other + 16u, 2u);
    }
}
class Diagnostic final : public family::DiagnosticServices
{
public:
    unsigned calls = 0;
    void Call(GuestAddress target, GuestMemory& m,
        family::Registers& s) override
    {
        if (target != 0x82f99d98u)
            throw std::runtime_error("unexpected diagnostic continuation");
        ++calls;
        s.sp += 96u;
        s.r[12] = m.ReadU32(recovery_abi::Address(s.sp - 8u));
        s.lr = s.r[12];
        s.r[3] = 0xfaceu;
        m.WriteU32(Output + 0x100u, 0xabcdu);
    }
};
void Check(const Case& item)
{
    std::fprintf(stderr, "case %s seed\n", item.name); std::fflush(stderr);
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item); auto state = FromPpc(context);
    Diagnostic diagnostic;
    ordinal_calls = feature_calls = normalize_calls = 0u;
    overlap_calls = walk_calls = layout_calls = 0u;
    classifier_calls = selector_calls = 0u;
    diagnostic_calls = diagnostic_lower_calls = 0u;
    std::fprintf(stderr, "case %s original start\n", item.name);
    std::fflush(stderr);
    __imp__sub_83058FC8(context, original.Bytes());
    std::fprintf(stderr, "case %s original done\n", item.name);
    std::fflush(stderr);
    if (!family::Apply(0x83058fc8u, right, diagnostic, state))
        throw std::runtime_error("semantic search caller entry missing");
    std::fprintf(stderr, "case %s semantic done\n", item.name);
    std::fflush(stderr);
    if (ordinal_calls == 0u ||
        (item.layout && layout_calls == 0u) ||
        (item.feature && feature_calls == 0u) ||
        (item.overlap && overlap_calls == 0u) ||
        diagnostic_calls != unsigned(item.diagnostic) ||
        diagnostic_lower_calls != unsigned(item.diagnostic) ||
        diagnostic.calls != unsigned(item.diagnostic))
    {
        std::fprintf(stderr,
            "path %s ordinal=%u feature=%u normalize=%u overlap=%u walk=%u layout=%u classifier=%u selector=%u diagnostic=%u/%u/%u\n",
            item.name, ordinal_calls, feature_calls, normalize_calls,
            overlap_calls, walk_calls, layout_calls, classifier_calls,
            selector_calls, diagnostic_calls, diagnostic_lower_calls,
            diagnostic.calls);
        throw std::runtime_error("original search caller fixture path");
    }
    const auto observed = FromPpc(context);
    if (!Same(observed, state) || !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,
            "mismatch %s r3=%016llX/%016llX SP=%016llX/%016llX LR=%016llX/%016llX CTR=%016llX/%016llX CA=%u/%u CR0=%u,%u,%u/%u,%u,%u CR6=%u,%u,%u/%u,%u,%u\n",
            item.name,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
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

void OriginalClassifier(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FB67E8(c, base); ++classifier_calls; }
void OriginalSelector(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FB6840(c, base); ++selector_calls; }
void OriginalLayout(PPCContext& c, std::uint8_t* base)
{ __imp__sub_83054648(c, base); ++layout_calls; }
void OriginalOrdinal(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FAC070(c, base); ++ordinal_calls; }
void OriginalFeature(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FAC608(c, base); ++feature_calls; }
void OriginalNormalize(PPCContext& c, std::uint8_t* base)
{ __imp__sub_83054600(c, base); ++normalize_calls; }
void OriginalOverlap(PPCContext& c, std::uint8_t* base)
{ __imp__sub_82FB71D0(c, base); ++overlap_calls; }
void OriginalWalk(PPCContext& c, std::uint8_t* base)
{ __imp__sub_83057B58(c, base); ++walk_calls; }
void OriginalDiagnostic(PPCContext& c, std::uint8_t* base)
{ ++diagnostic_calls; __imp__sub_82F99F48(c, base); }
void OriginalDiagnosticLower(PPCContext& c, std::uint8_t* base)
{
    ++diagnostic_lower_calls;
    c.r1.u64 += 96u;
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
    c.r3.u64 = 0xfaceu;
    PPC_STORE_U32(Output + 0x100u, 0xabcdu);
}
void OriginalSave(PPCContext& c, std::uint8_t* base, unsigned first)
{
    const auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        PPC_STORE_U64(c.r1.u32 - (33u - i) * 8u, fields[i]->u64);
    PPC_STORE_U32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore(PPCContext& c, std::uint8_t* base, unsigned first)
{
    const auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        fields[i]->u64 = PPC_LOAD_U64(c.r1.u32 - (33u - i) * 8u);
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
void OriginalSave25(PPCContext& c, std::uint8_t* b)
{ OriginalSave(c, b, 25u); }
void OriginalRestore25(PPCContext& c, std::uint8_t* b)
{ OriginalRestore(c, b, 25u); }
void OriginalSave26(PPCContext& c, std::uint8_t* b)
{ OriginalSave(c, b, 26u); }
void OriginalRestore26(PPCContext& c, std::uint8_t* b)
{ OriginalRestore(c, b, 26u); }

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-search-caller %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size());
        std::puts("LIMIT selected ordinary RAM paths; diagnostic 82F99D98 is mutable boundary; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
