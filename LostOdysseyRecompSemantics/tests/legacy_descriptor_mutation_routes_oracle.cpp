#include "lo_semantics/legacy_descriptor_mutation_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_mutation_routes;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Object = 0x10000u;
constexpr GuestAddress Buffer = 0x30000u;
constexpr GuestAddress Context = 0x40000u;
constexpr GuestAddress JumpTable = 0x82174e48u;
constexpr GuestAddress SelectorTable = 0x821712a0u;
constexpr std::array<test::Region, 3> Regions{{
    {0u, 0x90000u}, {0x82171000u, 0x1000u},
    {0x82174000u, 0x1000u}
}};

// Exact image_disc1.bin bytes at file offsets 0x174E48..0x174EA8,
// guest VA 0x82174E48..0x82174EA8 (VA - 0x82000000).
constexpr std::array<std::uint8_t, 97> JumpBytes{{
    0x00, 0x00, 0x00, 0x00, 0xe8, 0x1c, 0x1c, 0xe8,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe0, 0xe0, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0x00,
    0x34, 0xe0, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe8, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x1c, 0x1c,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0x4c, 0x4c,
    0x4c, 0xe0, 0xe0, 0xe8, 0xe8, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x4c, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0x64, 0x64, 0xe8, 0xe8, 0xe8, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe8, 0x74, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0,
    0xe0
}};
// Accepted 82FAC238 lower reads VA 0x821712A0..C9, image file
// offsets 0x1712A0..C9. This is a validation-only original body here.
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};

struct Case
{
    const char* name;
    std::uint32_t code, initial_flags, stack_mode;
    std::uint32_t expected_selector_calls;
};
constexpr std::array<Case, 9> Cases{{
    {"code5-flag-group-full", 5u, 3510u, 2u, 0u},
    {"code10-flag-group", 10u, 0u, 3u, 0u},
    {"code29-small-mask", 29u, 0u, 4u, 0u},
    {"code51-flag-group", 51u, 0u, 2u, 0u},
    {"code85-buffer-flag", 85u, 0u, 3u, 0u},
    {"code95-record-byte", 95u, 0u, 4u, 1u},
    {"code96-record-header", 96u, 0u, 2u, 1u},
    {"code101-descriptor-bit", 101u, 0u, 3u, 0u},
    {"code103-out-of-range", 103u, 0u, 4u, 0u}
}};
unsigned selector_calls = 0;
unsigned mutation_calls = 0;

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
    c.r4.u64 = 0x5566778800000000ull | Buffer;
    c.r5.u64 = 0x99aabbcc00000000ull |
        (entry == 0x83056ac0u ? item.code : Context);
    c.r6.u64 = 0xddeeffaa00000000ull |
        (entry == 0x83056ac0u ? Context : item.code);
    c.r7.u64 = 0x1011121300001234ull;
    c.r8.u64 = 0x2021222300000056ull;
    c.r9.u64 = 0x303132330000789aull;
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
    for (unsigned index = 0; index < SelectorBytes.size(); ++index)
        memory.WriteU8(SelectorTable + index, SelectorBytes[index]);
    // The previous descriptor value supplies preserved bits. Both entry
    // bodies derive the selector's new type/slot from the input event.
    memory.WriteU32(Object + 8u, (83u << 7u) | (1u << 19u) |
        (2u << 14u) | 2u);
    memory.WriteU32(Object + 16u, item.initial_flags);
    memory.WriteU32(Buffer + 40u, 0x10203040u);
    memory.WriteU32(Stack + 84u, 0x000000c3u);
    memory.WriteU32(Stack + 92u, item.stack_mode);
    for (unsigned offset = 32u; offset <= 112u; offset += 4u)
        memory.WriteU32(Object + offset, 0x0a0b0000u | offset);
}

void CallOriginal(GuestAddress entry, PPCContext& c, std::uint8_t* base)
{
    switch (entry)
    {
    case 0x83056568u: __imp__sub_83056568(c, base); return;
    case 0x83056ac0u: __imp__sub_83056AC0(c, base); return;
    default: throw std::runtime_error("unknown descriptor mutation entry");
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
    selector_calls = 0; mutation_calls = 0;
    CallOriginal(entry, context, original.Bytes());
    if (!family::Apply(entry, right, state))
        throw std::runtime_error("recovered descriptor mutation entry missing");
    const auto observed = FromPpc(context);
    const unsigned expected_selector_calls = item.expected_selector_calls +
        (entry == 0x83056ac0u ? 1u : 0u);
    if (selector_calls != expected_selector_calls ||
        mutation_calls != (entry == 0x83056ac0u ? 1u : 0u))
        throw std::runtime_error("original descriptor mutation fixture path");
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
    if (c.lr != 0x83056664u && c.lr != 0x83056690u &&
        c.lr != 0x83056b18u)
        throw std::runtime_error("actual 82FAC238 call LR");
    __imp__sub_82FAC238(c, base);
    ++selector_calls;
}
void OriginalMutate(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83056b00u)
        throw std::runtime_error("actual 83056568 call LR");
    __imp__sub_83056568(c, base);
    ++mutation_calls;
}
void OriginalSave27(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        PPC_STORE_U64(c.r1.u32 - 48u + (index - 27u) * 8u,
            fields[index]->u64);
    PPC_STORE_U32(c.r1.u32 - 8u, c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t* base)
{
    const auto fields = Fields(c);
    for (unsigned index = 27u; index <= 31u; ++index)
        fields[index]->u64 = PPC_LOAD_U64(
            c.r1.u32 - 48u + (index - 27u) * 8u);
    c.r12.u64 = PPC_LOAD_U32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}

int main()
{
    try
    {
        for (const auto& item : Cases)
        {
            Check(item, 0x83056568u);
            Check(item, 0x83056ac0u);
        }
        std::printf("PASS legacy-descriptor-mutation-routes %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size() * 2u);
        std::puts("LIMIT selected fixed-image jump tables and ordinary RAM; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
