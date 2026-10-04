#include "lo_semantics/legacy_descriptor_bit_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_bit_routes;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Record = 0x20000u;
constexpr GuestAddress Node = 0x30000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x90000u}}};
struct Case
{
    const char* name;
    std::uint32_t type, count, packed, free_bits, slot_count;
    bool active_node;
    std::uint32_t expected_class;
};
constexpr std::array<Case, 8> Cases{{
    {"range-31-early", 31u, 3u, 0u, 0u, 0u, false, 1u},
    {"range-16-early", 16u, 1u, 0u, 0u, 0u, false, 1u},
    {"range-24-early", 24u, 2u, 0u, 0u, 0u, false, 1u},
    {"type-82-empty", 82u, 0u, 0u, 0u, 0u, false, 0u},
    {"one-slot-bit", 0u, 1u, 0u, 1u, 0u, false, 0u},
    {"three-free-bits", 0u, 1u, 0u, 3u, 0u, false, 0u},
    {"record-propagation", 0u, 1u, 0u, 3u, 1u, false, 0u},
    {"record-and-active-node", 0u, 1u, 0u, 3u, 1u, true, 0u}
}};
unsigned original_classifier_calls = 0;

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
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1234567800000000ull | Object;
    c.r4.u64 = 0x5566778800000000ull | item.count;
    c.r5.u64 = 0x99aabbcc00000000ull | item.packed;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    return c;
}
void Seed(GuestMemory& memory, const Case& item)
{
    const std::uint32_t descriptor = (item.type << 7u) |
        (item.free_bits << 1u) | (item.slot_count << 19u);
    memory.WriteU32(Object + 8u, descriptor);
    memory.WriteU32(Object + 4u, item.active_node ? Node : 0u);
    memory.WriteU32(Object + 40u, Record);
    memory.WriteU32(Record, 0x12345678u);
    memory.WriteU32(Node, 0x0a345678u);
    memory.WriteU32(Node + 8u, 0u);
    memory.WriteU32(Node + 16u, item.active_node ? 1u : 0u);
}
void CallOriginal(GuestAddress entry, PPCContext& c, std::uint8_t* base)
{
    switch (entry)
    {
    case 0x83053308u: __imp__sub_83053308(c, base); return;
    case 0x83054390u: __imp__sub_83054390(c, base); return;
    default: throw std::runtime_error("unknown descriptor bit route");
    }
}
void Check(const Case& item, GuestAddress entry)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item);
    auto state = FromPpc(context);
    original_classifier_calls = 0;
    CallOriginal(entry, context, original.Bytes());
    if (!family::Apply(entry, right, state))
        throw std::runtime_error("recovered descriptor bit route missing");
    const auto observed = FromPpc(context);
    if (original_classifier_calls != (entry == 0x83054390u ? 1u : 0u) ||
        (entry == 0x83053308u &&
            static_cast<std::uint32_t>(observed.r[3]) != item.expected_class))
        throw std::runtime_error("original descriptor fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered))
    {
        std::fprintf(stderr, "mismatch %s entry=%08X r3=%016llX/%016llX r11=%016llX/%016llX SP=%016llX/%016llX LR=%016llX/%016llX CA=%u/%u\n",
            item.name, entry,
            static_cast<unsigned long long>(observed.r[3]),
            static_cast<unsigned long long>(state.r[3]),
            static_cast<unsigned long long>(observed.r[11]),
            static_cast<unsigned long long>(state.r[11]),
            static_cast<unsigned long long>(observed.sp),
            static_cast<unsigned long long>(state.sp),
            static_cast<unsigned long long>(observed.lr),
            static_cast<unsigned long long>(state.lr),
            observed.xer_ca, state.xer_ca);
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

void OriginalDescriptorClass(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x830543acu)
        throw std::runtime_error("actual 83053308 call LR");
    __imp__sub_83053308(c, base);
    ++original_classifier_calls;
}

void OriginalSave28(PPCContext& c, std::uint8_t* base)
{
    PPC_STORE_U64(c.r1.u32 - 40u, c.r28.u64);
    PPC_STORE_U64(c.r1.u32 - 32u, c.r29.u64);
    PPC_STORE_U64(c.r1.u32 - 24u, c.r30.u64);
    PPC_STORE_U64(c.r1.u32 - 16u, c.r31.u64);
    PPC_STORE_U32(c.r1.u32 - 8u, c.r12.u32);
}

void OriginalRestore28(PPCContext& c, std::uint8_t* base)
{
    c.r28.u64 = PPC_LOAD_U64(c.r1.u32 - 40u);
    c.r29.u64 = PPC_LOAD_U64(c.r1.u32 - 32u);
    c.r30.u64 = PPC_LOAD_U64(c.r1.u32 - 24u);
    c.r31.u64 = PPC_LOAD_U64(c.r1.u32 - 16u);
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}

int main()
{
    try
    {
        for (const auto& item : Cases)
        {
            Check(item, 0x83053308u);
            Check(item, 0x83054390u);
        }
        std::printf("PASS legacy-descriptor-bit-routes %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size() * 2u);
        std::puts("LIMIT selected descriptor and ordered ordinary-RAM graph; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
