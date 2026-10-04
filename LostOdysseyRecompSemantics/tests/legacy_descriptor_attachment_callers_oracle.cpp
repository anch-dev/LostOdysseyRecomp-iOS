#include "lo_semantics/legacy_descriptor_attachment_callers.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_attachment_callers;
constexpr GuestAddress Stack = 0x80000u, Owner = 0x10000u;
constexpr GuestAddress Name = 0x20000u, Node = 0x30000u;
constexpr GuestAddress Attachment = 0x50020u, Second = 0x52020u;
constexpr GuestAddress AttachmentOwner = 0x54000u, Metadata = 0x58000u;
constexpr GuestAddress Candidate = 0x42020u, Pool = 0x60000u;
constexpr GuestAddress Previous = 0x70000u;
constexpr GuestAddress SelectorTable = 0x821712a0u;
constexpr GuestAddress JumpTable = 0x82174e48u;
constexpr std::array<test::Region, 3> Regions{{
    {0u, 0x90000u}, {0x82171000u, 0x1000u}, {0x82174000u, 0x1000u}
}};
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};
// image_disc1.bin VA 82174E48..E4EA8, file offsets 00174E48..00174EA8.
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
struct Case
{
    const char* name;
    GuestAddress entry;
    bool owned;
    bool low_bit;
    bool prelinked;
};
constexpr std::array Cases{
    Case{"first-unowned", 0x83057b90u, false, false, false},
    Case{"first-owned-copy", 0x83057b90u, true, false, false},
    Case{"first-low-bit-owned", 0x83057b90u, true, true, false},
    Case{"first-ring-existing", 0x83057b90u, false, false, true},
    Case{"second-unowned", 0x83057fb0u, false, false, false},
    Case{"second-owned-copy", 0x83057fb0u, true, false, false},
    Case{"second-low-bit-packed", 0x83057fb0u, true, true, false},
    Case{"second-ring-existing", 0x83057fb0u, false, false, true}
};
unsigned allocate_calls = 0, copy_calls = 0, attach_calls = 0;
unsigned value_calls = 0, pool_calls = 0;

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers s{}; const auto fields = Fields(c);
    auto& g = s.integer;
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) g.r[i] = fields[i]->u64;
    g.sp = c.r1.u64; g.lr = c.lr; g.ctr = c.ctr.u64;
    g.xer_so = c.xer.so; g.xer_ca = c.xer.ca;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    s.f0_bits = c.f0.u64; s.f1_bits = c.f1.u64; s.f2_bits = c.f2.u64;
    s.f3_bits = c.f3.u64; s.f4_bits = c.f4.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.cached_fp_control = c.fpscr.csr;
    return s;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }
bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.integer; const auto& y = b.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.f0_bits == b.f0_bits && a.f1_bits == b.f1_bits &&
        a.f2_bits == b.f2_bits && a.f3_bits == b.f3_bits &&
        a.f4_bits == b.f4_bits && a.f12_bits == b.f12_bits &&
        a.f13_bits == b.f13_bits &&
        a.cached_fp_control == b.cached_fp_control;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{}; auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1122334400000000ull + i;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = Owner; c.r4.u64 = Name;
    c.r5.u64 = item.entry == 0x83057b90u ? Attachment : Candidate;
    c.r6.u64 = Second;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5566778899aabbccull;
    c.cr0.gt = 1; c.cr6.lt = 1; c.xer.so = 1; c.xer.ca = 1;
    c.f0.u64 = std::bit_cast<std::uint64_t>(5.0);
    c.f1.u64 = std::bit_cast<std::uint64_t>(1.0);
    c.f2.u64 = std::bit_cast<std::uint64_t>(2.0);
    c.f3.u64 = std::bit_cast<std::uint64_t>(3.0);
    c.f4.u64 = std::bit_cast<std::uint64_t>(4.0);
    c.f12.u64 = std::bit_cast<std::uint64_t>(7.0);
    c.f13.u64 = std::bit_cast<std::uint64_t>(9.0);
    c.fpscr.csr = 0x1f80u;
    return c;
}
void Seed(GuestMemory& m, const Case& item)
{
    for (unsigned i = 0; i < SelectorBytes.size(); ++i)
        m.WriteU8(SelectorTable + i, SelectorBytes[i]);
    for (unsigned i = 0; i < JumpBytes.size(); ++i)
        m.WriteU8(JumpTable + i, JumpBytes[i]);
    m.WriteU32(Owner + 4u, Owner);
    m.WriteU32(Owner + 16u, Previous);
    m.WriteU32(16u, Previous); m.WriteU32(Previous, 0u);
    m.WriteU32(Owner + 40u, 0u);
    m.WriteU32(Owner + 912u, Pool + 4096u);
    m.WriteU32(Owner + 916u, Pool);
    m.WriteU32(Name + 24u, Node);
    m.WriteU32(Node, item.prelinked ? Attachment : 0u);
    m.WriteU32(Attachment, 0x11223305u);
    m.WriteU32(Attachment + 4u, 0u);
    m.WriteU32(Attachment + 8u, 0x44556676u | (item.low_bit ? 1u : 0u));
    m.WriteU32(Attachment + 12u, AttachmentOwner);
    m.WriteU32(Attachment + 16u, item.owned ? Previous : 0u);
    m.WriteU32(Second, 0x11223305u);
    m.WriteU32(Second + 4u, 0u);
    m.WriteU32(Second + 8u, 0x44556676u | (item.low_bit ? 1u : 0u));
    m.WriteU32(Second + 12u, AttachmentOwner);
    m.WriteU32(Second + 16u, item.owned ? Previous : 0u);
    m.WriteU32(AttachmentOwner + 4u, 0u);
    m.WriteU32(0x50000u, Metadata);
    m.WriteU32(0x52000u, Metadata);
    m.WriteU32(Metadata + 148u, Owner);
    m.WriteU32(Candidate, 0u); m.WriteU32(Candidate + 4u, 0u);
    m.WriteU32(Candidate + 8u, item.low_bit ? 1u : 0u);
}
struct Services final : family::Services
{
    void AllocateFromPool(GuestMemory&, crt_stream_operations::Registers&) override
    { std::abort(); }
    void SetHostFpControl(std::uint32_t csr) override
    { simde_mm_setcsr(csr); }
};
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item); auto state = FromPpc(context);
    Services services;
    allocate_calls = copy_calls = attach_calls = value_calls = pool_calls = 0;
    const auto previous_control = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    std::fprintf(stderr, "attachment %s original\n", item.name);
    std::fflush(stderr);
    if (item.entry == 0x83057b90u)
        __imp__sub_83057B90(context, original.Bytes());
    else __imp__sub_83057FB0(context, original.Bytes());
    const auto observed = FromPpc(context);
    const auto original_control = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    std::fprintf(stderr, "attachment %s recovered\n", item.name);
    std::fflush(stderr);
    if (!family::Apply(item.entry, right, services, state))
        throw std::runtime_error("attachment implementation missing");
    const auto recovered_control = simde_mm_getcsr();
    simde_mm_setcsr(previous_control);
    if (allocate_calls != 1u ||
        value_calls != (item.entry == 0x83057fb0u ? 1u : 0u) ||
        attach_calls != (item.entry == 0x83057fb0u ? 2u : 1u) ||
        copy_calls != (item.owned ? 1u : 0u) ||
        pool_calls != (item.entry == 0x83057fb0u ? 2u : 1u) +
            (item.owned ? 1u : 0u))
    {
        std::fprintf(stderr, "%s paths allocate=%u value=%u attach=%u copy=%u pool=%u\n",
            item.name, allocate_calls, value_calls, attach_calls,
            copy_calls, pool_calls);
        throw std::runtime_error("actual attachment path");
    }
    if (observed.integer.r[3] == 0u || left.ReadU32(Name + 24u) == Node)
        throw std::runtime_error("ring link result");
    if (item.entry == 0x83057fb0u &&
        (left.ReadU32(static_cast<GuestAddress>(observed.integer.r[3]) + 40u) == 0u ||
            left.ReadU32(static_cast<GuestAddress>(observed.integer.r[3]) + 44u) == 0u))
        throw std::runtime_error("second attachment fields");
    if (!Same(observed, state) || original_control != recovered_control ||
        !original.EqualCommitted(recovered))
    {
        for (unsigned i = 0; i < 32u; ++i)
            if (observed.integer.r[i] != state.integer.r[i])
                std::fprintf(stderr, "r%u %016llX/%016llX\n", i,
                    (unsigned long long)observed.integer.r[i],
                    (unsigned long long)state.integer.r[i]);
        std::fprintf(stderr, "%s SP %llX/%llX LR %llX/%llX FP0 %llX/%llX CSR %08X/%08X\n",
            item.name, (unsigned long long)observed.integer.sp,
            (unsigned long long)state.integer.sp,
            (unsigned long long)observed.integer.lr,
            (unsigned long long)state.integer.lr,
            (unsigned long long)observed.f0_bits,
            (unsigned long long)state.f0_bits,
            original_control, recovered_control);
        for (auto region : Regions)
            for (std::size_t off = 0; off < region.size; ++off)
            {
                const auto address = std::size_t(region.base) + off;
                if (original.Bytes()[address] != recovered.Bytes()[address])
                {
                    std::fprintf(stderr, "RAM %08zX %02X/%02X\n",
                        address, original.Bytes()[address],
                        recovered.Bytes()[address]);
                    throw std::runtime_error(item.name);
                }
            }
        throw std::runtime_error(item.name);
    }
}
} // namespace

void OriginalBody83056B90(PPCContext& c, std::uint8_t* b)
{ ++allocate_calls; __imp__sub_83056B90(c, b); }
void OriginalBody82FB6B28(PPCContext& c, std::uint8_t* b)
{ ++copy_calls; __imp__sub_82FB6B28(c, b); }
void OriginalBody82FBDA20(PPCContext& c, std::uint8_t* b)
{ ++attach_calls; __imp__sub_82FBDA20(c, b); }
void OriginalBody82FBD850(PPCContext& c, std::uint8_t* b)
{ ++value_calls; __imp__sub_82FBD850(c, b); }
void OriginalBody82FB36B0(PPCContext& c, std::uint8_t* b)
{ ++pool_calls; __imp__sub_82FB36B0(c, b); }
void OriginalNativeAllocate(PPCContext&, std::uint8_t*) { std::abort(); }

namespace
{
void Save(unsigned first, PPCContext& c, std::uint8_t* b)
{
    auto m = GuestMemory(0u, std::span<std::uint8_t>(b, test::GuestWindow::Space));
    const auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        recovery_abi::WriteU64(m, c.r1.u32 - 8u * (33u - i), fields[i]->u64);
    m.WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void Restore(unsigned first, PPCContext& c, std::uint8_t* b)
{
    auto m = GuestMemory(0u, std::span<std::uint8_t>(b, test::GuestWindow::Space));
    const auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        fields[i]->u64 = recovery_abi::ReadU64(
            m, c.r1.u32 - 8u * (33u - i));
    c.r12.u64 = m.ReadU32(c.r1.u32 - 8u); c.lr = c.r12.u64;
}
}
void OriginalSave23(PPCContext& c, std::uint8_t* b) { Save(23u, c, b); }
void OriginalRestore23(PPCContext& c, std::uint8_t* b) { Restore(23u, c, b); }
void OriginalSave25(PPCContext& c, std::uint8_t* b) { Save(25u, c, b); }
void OriginalRestore25(PPCContext& c, std::uint8_t* b) { Restore(25u, c, b); }
void OriginalSave27(PPCContext& c, std::uint8_t* b) { Save(27u, c, b); }
void OriginalRestore27(PPCContext& c, std::uint8_t* b) { Restore(27u, c, b); }
void OriginalSave28(PPCContext& c, std::uint8_t* b) { Save(28u, c, b); }
void OriginalRestore28(PPCContext& c, std::uint8_t* b) { Restore(28u, c, b); }
void OriginalSave29(PPCContext& c, std::uint8_t* b) { Save(29u, c, b); }
void OriginalRestore29(PPCContext& c, std::uint8_t* b) { Restore(29u, c, b); }

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-attachment-callers %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected ordinary RAM and attachment layouts; native exhausted pool, other context, faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
