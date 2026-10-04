#include "lo_semantics/legacy_descriptor_record_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_record_routes;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Node = 0x20000u;
constexpr GuestAddress Context = 0x30000u;
constexpr GuestAddress Child = 0x40000u;
constexpr GuestAddress Subtype = 0x50000u;
constexpr GuestAddress Output = 0x60000u;
constexpr GuestAddress JumpTable = 0x821712a0u;
constexpr std::array<test::Region, 2> Regions{{
    {0u, 0x90000u}, {0x82171000u, 0x1000u}
}};

// Exact bytes from image_disc1.bin, file offsets 0x1712A0..0x1712C9,
// guest VA 0x821712A0..0x821712C9 (VA - 0x82000000).
constexpr std::array<std::uint8_t, 42> JumpBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};

struct Case
{
    const char* name;
    std::uint32_t type, selector_extra, slot, field6, field_mask;
    std::uint32_t record_extra, node_flags, node_active;
    std::uint32_t child_flags, subtype_type, context_flags;
};

constexpr std::array<Case, 8> Cases{{
    {"type83-empty", 83u, 28u, 0u, 0u, 0u, 0u, 0u, 0u,
        0u, 95u, 0u},
    {"type84-skip-node", 84u, 16u, 0u, 0u, 0xau, 0x0100u,
        0x08000010u, 0u, 0u, 95u, 0u},
    {"type95-active-low", 95u, 20u, 0u, 0u, 0x5u, 0x2100u,
        0x08000010u, 1u, 0u, 95u, 0u},
    {"type96-active-high", 96u, 24u, 0u, 0u, 0xfu, 0x8300u,
        0x0a000000u, 1u, 0u, 95u, 0u},
    {"type83-child-flag", 83u, 28u, 1u, 0u, 0x3u, 0x0400u,
        0x08000010u, 1u, 0x0000013fu, 95u, 0u},
    {"type95-subtype", 95u, 20u, 2u, 0u, 0x9u, 0x5100u,
        0x08000010u, 1u, 0x0000012fu, 89u, 0u},
    {"type124-variable-status", 124u, 20u, 2u, 5u, 0xbu,
        0x9000u, 0x08000010u, 1u, 0x000000cfu, 92u, 0x400000u},
    {"type125-out-of-range", 125u, 0u, 7u, 0u, 0x2u, 0u,
        0u, 0u, 0u, 95u, 0u}
}};

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

PPCContext Initial(const Case& item, GuestAddress entry)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1234567800000000ull | Object;
    c.r4.u64 = 0x5566778800000000ull | Output;
    c.r5.u64 = 0x99aabbcc00000000ull | 3u;
    if (entry == 0x82fac238u)
    {
        c.r3.u64 = 0x1234567800000003ull;
        c.r4.u64 = 0x5566778800000000ull | item.type;
        c.r5.u64 = 0x99aabbcc00000000ull | item.slot;
        c.r6.u64 = 0xddeeffaa00000000ull | item.field6;
    }
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    return c;
}

void Seed(GuestMemory& memory, const Case& item)
{
    for (unsigned index = 0; index < JumpBytes.size(); ++index)
        memory.WriteU8(JumpTable + index, JumpBytes[index]);
    const auto descriptor = (item.type << 7u) | (item.slot << 19u) |
        (item.field6 << 14u) | (item.field_mask << 1u);
    memory.WriteU32(Object + 8u, descriptor);
    memory.WriteU32(Object + 4u, item.node_flags ? Node : 0u);
    memory.WriteU32(Object + 24u, Context);
    memory.WriteU32(Object + 40u, Child);
    memory.WriteU32(Object + 44u, Subtype);
    memory.WriteU32(Node, item.node_flags);
    memory.WriteU32(Node + 8u, 0u);
    memory.WriteU32(Node + 16u, item.node_active);
    memory.WriteU32(Child, item.child_flags);
    memory.WriteU32(Subtype, item.subtype_type << 17u);
    memory.WriteU32(Context + 76u, item.context_flags);
    const auto record = Object + 20u + item.slot * 4u +
        item.selector_extra;
    memory.WriteU32(record, 0x12345678u);
    memory.WriteU32(record + 4u, 0x0a0b0c0du);
    memory.WriteU32(record + 8u, 0x98765432u);
    memory.WriteU32(record + 12u, item.record_extra);
}

void CallOriginal(GuestAddress entry, PPCContext& c, std::uint8_t* base)
{
    switch (entry)
    {
    case 0x82fac238u: __imp__sub_82FAC238(c, base); return;
    case 0x830555e0u: __imp__sub_830555E0(c, base); return;
    default: throw std::runtime_error("unknown descriptor record entry");
    }
}

void Check(const Case& item, GuestAddress entry)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item, entry);
    auto state = FromPpc(context);
    selector_calls = 0;
    CallOriginal(entry, context, original.Bytes());
    if (!family::Apply(entry, right, state))
        throw std::runtime_error("recovered descriptor record entry missing");
    const auto observed = FromPpc(context);
    if (selector_calls != (entry == 0x830555e0u ? 1u : 0u) ||
        (entry == 0x82fac238u &&
            static_cast<std::uint32_t>(observed.r[3]) !=
                (item.slot + 10u) * 4u + item.selector_extra))
        throw std::runtime_error("original descriptor selector fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr,
            "mismatch %s entry=%08X r3=%016llX/%016llX r11=%016llX/%016llX SP=%016llX/%016llX LR=%016llX/%016llX CTR=%016llX/%016llX CA=%u/%u CR0=%u,%u,%u/%u,%u,%u CR6=%u,%u,%u/%u,%u,%u\n",
            item.name, entry,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[11]),
            static_cast<unsigned long long>(state.r[11]),
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
        for (unsigned index = 0; index < 32u; ++index)
            if (observed.r[index] != state.r[index])
                std::fprintf(stderr, "r%u=%016llX/%016llX\n", index,
                    static_cast<unsigned long long>(observed.r[index]),
                    static_cast<unsigned long long>(state.r[index]));
        for (const auto region : Regions)
            for (std::size_t offset = 0; offset < region.size; ++offset)
            {
                const auto address = static_cast<std::size_t>(region.base) + offset;
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

void OriginalRecordSelector(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83055614u)
        throw std::runtime_error("actual 82FAC238 call LR");
    __imp__sub_82FAC238(c, base);
    ++selector_calls;
}

int main()
{
    try
    {
        for (const auto& item : Cases)
        {
            Check(item, 0x82fac238u);
            Check(item, 0x830555e0u);
        }
        std::printf("PASS legacy-descriptor-record-routes %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size() * 2u);
        std::puts("LIMIT selected immutable image jump table and ordinary-RAM descriptor graphs; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
