#include "lo_semantics/legacy_descriptor_layout_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_layout_routes;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Descriptor = 0x20000u;
constexpr GuestAddress ChildType = 0x30000u;
constexpr GuestAddress Flags = 0x40000u;
constexpr GuestAddress Output = 0x50000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x90000u}}};

struct Case
{
    const char* name;
    std::uint32_t type, slots, count, pattern, descriptor_flags, child_type;
    std::uint32_t classifier_result, selector_result;
};

constexpr std::array<Case, 8> Cases{{
    {"type-zero-empty", 0u, 0u, 0u, 0u, 0u, 123u, 0u, 0u},
    {"type-one-special-empty", 1u, 0u, 0u, 0u, 0u, 124u, 1u, 1u},
    {"type-one-special-packed", 1u, 5u, 3u, 0xe4u, 0x1eu, 123u, 1u, 1u},
    {"type-sixteen-general", 16u, 3u, 2u, 0x39u, 0x14u, 124u, 0u, 0u},
    {"type-twentyfive-excluded", 25u, 4u, 4u, 0xe4u, 0x8u, 122u, 1u, 0u},
    {"type-twentyeight-excluded", 28u, 3u, 2u, 0x18u, 0x6u, 123u, 1u, 0u},
    {"type-twentynine-excluded", 29u, 1u, 1u, 0x3u, 0x10u, 122u, 0u, 0u},
    {"type-thirty-special", 30u, 10u, 4u, 0xb4u, 0x1au, 124u, 1u, 1u}
}};

unsigned classifier_calls = 0;
unsigned selector_calls = 0;

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}

family::Registers FromPpc(PPCContext& c)
{
    family::Registers state{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        state.r[index] = index == 1u ? 0u : fields[index]->u64;
    state.sp = c.r1.u64; state.lr = c.lr; state.ctr = c.ctr.u64;
    state.xer_so = c.xer.so; state.xer_ca = c.xer.ca;
    state.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    state.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    return state;
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

PPCContext Initial()
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1234567800000000ull | Object;
    c.r4.u64 = 0x5566778800000000ull;
    c.r5.u64 = 0x99aabbcc00000000ull | Flags;
    c.r7.u64 = 0xddeeffaa00000000ull | Output;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    return c;
}

void Seed(GuestMemory& memory, const Case& item)
{
    memory.WriteU32(Object + 8u, (item.type << 7u) |
        (item.slots << 1u));
    memory.WriteU32(Object + 40u, Descriptor);
    memory.WriteU32(Descriptor, (item.count << 25u) |
        (item.pattern << 5u) | item.descriptor_flags);
    memory.WriteU32(Descriptor + 12u, ChildType);
    memory.WriteU32(ChildType + 8u, item.child_type << 7u);
    memory.WriteU32(Flags, 0x12340000u);
    memory.WriteU32(Flags + 4u, 0x01020304u);
    memory.WriteU32(Output, 0x65430000u);
}

void CallOriginal(GuestAddress entry, PPCContext& c, std::uint8_t* base)
{
    switch (entry)
    {
    case 0x82fb67e8u: __imp__sub_82FB67E8(c, base); return;
    case 0x82fb6840u: __imp__sub_82FB6840(c, base); return;
    case 0x83054648u: __imp__sub_83054648(c, base); return;
    default: throw std::runtime_error("unknown descriptor layout entry");
    }
}

void Check(const Case& item, GuestAddress entry)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial();
    auto state = FromPpc(context);
    classifier_calls = 0; selector_calls = 0;
    CallOriginal(entry, context, original.Bytes());
    if (!family::Apply(entry, right, state))
        throw std::runtime_error("recovered descriptor layout entry missing");
    const auto observed = FromPpc(context);
    const unsigned expected_class_calls = entry == 0x82fb67e8u ? 0u : 1u;
    const unsigned expected_selector_calls = entry == 0x83054648u ? 1u : 0u;
    const auto expected_result = entry == 0x82fb67e8u ?
        item.classifier_result : entry == 0x82fb6840u ?
        item.selector_result : (item.child_type == 123u || item.child_type == 124u);
    if (classifier_calls != expected_class_calls ||
        selector_calls != expected_selector_calls ||
        static_cast<std::uint32_t>(observed.r[3]) != expected_result)
        throw std::runtime_error("original descriptor layout fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,
            "mismatch %s entry=%08X r3=%016llX/%016llX r11=%016llX/%016llX SP=%016llX/%016llX LR=%016llX/%016llX CA=%u/%u CR0=%u,%u,%u/%u,%u,%u CR6=%u,%u,%u/%u,%u,%u\n",
            item.name, entry,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[11]),
            static_cast<unsigned long long>(state.r[11]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            observed.xer_ca, state.xer_ca,
            observed.cr0.lt, observed.cr0.gt, observed.cr0.eq,
            state.cr0.lt, state.cr0.gt, state.cr0.eq,
            observed.cr6.lt, observed.cr6.gt, observed.cr6.eq,
            state.cr6.lt, state.cr6.gt, state.cr6.eq);
        for (unsigned index = 0; index < 32u; ++index)
            if (observed.r[index] != state.r[index])
                std::fprintf(stderr, "r%u=%016llX/%016llX\n", index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        for (const auto region : Regions)
            for (std::size_t offset = 0; offset < region.size; ++offset)
            {
                const auto address = region.base + offset;
                if (original.Bytes()[address] != recovered.Bytes()[address])
                {
                    std::fprintf(stderr, "RAM first difference %08zX=%02X/%02X\n",
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
{
    if (c.lr != 0x82fb6858u && c.lr != 0x83054698u)
        throw std::runtime_error("actual 82FB67E8 call LR");
    __imp__sub_82FB67E8(c, base);
    ++classifier_calls;
}

void OriginalSelector(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83054698u)
        throw std::runtime_error("actual 82FB6840 call LR");
    __imp__sub_82FB6840(c, base);
    ++selector_calls;
}

void OriginalSave25(PPCContext& c, std::uint8_t* base)
{
    for (unsigned index = 25u; index <= 31u; ++index)
        PPC_STORE_U64(c.r1.u32 - 64u + (index - 25u) * 8u,
            Fields(c)[index]->u64);
    PPC_STORE_U32(c.r1.u32 - 8u, c.r12.u32);
}

void OriginalRestore25(PPCContext& c, std::uint8_t* base)
{
    for (unsigned index = 25u; index <= 31u; ++index)
        Fields(c)[index]->u64 = PPC_LOAD_U64(
            c.r1.u32 - 64u + (index - 25u) * 8u);
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}

int main()
{
    try
    {
        for (const auto& item : Cases)
        {
            Check(item, 0x82fb67e8u);
            Check(item, 0x82fb6840u);
            Check(item, 0x83054648u);
        }
        std::printf("PASS legacy-descriptor-layout-routes %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size() * 3u);
        std::puts("LIMIT selected descriptor/type graph in ordinary RAM; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
