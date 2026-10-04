#include "lo_semantics/legacy_descriptor_array_callers.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_array_callers;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Owner = 0x10000u;
constexpr GuestAddress Previous = 0x20000u;
constexpr GuestAddress Input = 0x30000u;
constexpr GuestAddress Pool = 0x60000u;
constexpr GuestAddress SelectorTable = 0x821712a0u;
constexpr std::array<test::Region, 3> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x1000u},
    {0x82171000u, 0x1000u}
}};
// image_disc1.bin VA 0x821712A0..C9, file offsets 0x1712A0..C9.
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
    GuestAddress entry;
    std::uint32_t count, code;
    std::uint64_t first, second;
    bool diagnostic;
};
constexpr std::array<Case, 8> Cases{{
    {"pairs-empty", 0x83058c60u, 0u, 0u, 0u, 0u, false},
    {"pairs-one-flags", 0x83058c60u, 1u, 3u,
        0x3fc00000u, 0u, false},
    {"pairs-two-bits", 0x83058c60u, 2u, 0xdu,
        0xbf000000u, 0x40100000u, false},
    {"typed-empty", 0x83058cf0u, 0u, 0u, 0u, 0u, false},
    {"typed-float", 0x83058cf0u, 1u, 0u,
        std::bit_cast<std::uint64_t>(1.5), 0u, false},
    {"typed-int32", 0x83058cf0u, 1u, 1u,
        std::bit_cast<std::uint64_t>(-1082130432.0), 0u, false},
    {"typed-int64", 0x83058cf0u, 1u, 2u,
        std::bit_cast<std::uint64_t>(1065353216.0), 0u, false},
    {"typed-diagnostic", 0x83058cf0u, 1u, 3u,
        std::bit_cast<std::uint64_t>(2.0), 0u, true}
}};

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
    auto& g = s.integer;
    for (unsigned i = 0u; i < 32u; ++i)
        g.r[i] = i == 1u ? 0u : fields[i]->u64;
    g.sp = c.r1.u64; g.lr = c.lr; g.ctr = c.ctr.u64;
    g.xer_so = c.xer.so; g.xer_ca = c.xer.ca;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    s.f0_bits = c.f0.u64; s.f12_bits = c.f12.u64;
    s.f13_bits = c.f13.u64; s.cached_fp_control = c.fpscr.csr;
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
        a.f0_bits == b.f0_bits && a.f12_bits == b.f12_bits &&
        a.f13_bits == b.f13_bits &&
        a.cached_fp_control == b.cached_fp_control;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1122334400000000ull + i;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0x1234567800000000ull | Owner;
    c.r4.u64 = 0x5566778800000000ull | item.count;
    c.r5.u64 = 0x99aabbcc00000000ull | Input;
    c.r6.u64 = 0xddeeffaa00000000ull | item.code;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    c.f0.u64 = std::bit_cast<std::uint64_t>(5.0);
    c.f12.u64 = std::bit_cast<std::uint64_t>(7.0);
    c.f13.u64 = std::bit_cast<std::uint64_t>(9.0);
    c.fpscr.csr = 0x1f80u;
    return c;
}
void Seed(GuestMemory& m, const Case& item)
{
    for (unsigned i = 0; i < SelectorBytes.size(); ++i)
        m.WriteU8(SelectorTable + i, SelectorBytes[i]);
    // image_disc1.bin VA 0x82000E50, file offset 0xE50: 00 00 00 00.
    m.WriteU32(0x82000e50u, 0u);
    m.WriteU32(Owner + 4u, Owner);
    m.WriteU32(Owner + 16u, Previous);
    m.WriteU32(Previous, 0u);
    m.WriteU32(Owner + 40u, 0u);
    m.WriteU32(Owner + 88u, 0u);
    m.WriteU32(Owner + 540u, 0u);
    m.WriteU32(Owner + 912u, Pool);
    m.WriteU32(Owner + 916u, Pool);
    for (std::uint32_t i = 0u; i < 7u; ++i)
        m.WriteU32(Owner + 60u + 4u * i, 0u);
    for (std::uint32_t i = 0u; i <= 2u; ++i)
        m.WriteU32(Owner + 772u + 40u + 4u * i - 4u, 0u);
    if (item.entry == 0x83058c60u)
    {
        m.WriteU32(Input, static_cast<std::uint32_t>(item.first));
        m.WriteU32(Input + 4u, item.code & 3u);
        m.WriteU32(Input + 8u, static_cast<std::uint32_t>(item.second));
        m.WriteU32(Input + 12u, (item.code >> 2u) & 3u);
    }
    else
    {
        recovery_abi::WriteU64(m, Input, item.first);
        recovery_abi::WriteU64(m, Input + 8u, item.second);
    }
}
class AllocationServices final : public legacy_descriptor_array_allocation::Services
{
public:
    unsigned pool = 0, fp = 0;
    void AllocateFromPool(GuestMemory&,
        crt_stream_operations::Registers&) override
    { ++pool; throw std::runtime_error("unexpected pool exhaustion"); }
    void SetHostFpControl(std::uint32_t csr) override
    { ++fp; simde_mm_setcsr(csr); }
};
class FreeServices final : public legacy_descriptor_array_lookup::FreeTailServices
{
public:
    void Call(GuestAddress, GuestMemory&,
        crt_stream_operations::Registers&) override
    { throw std::runtime_error("unexpected free boundary"); }
};
class DiagnosticServices final : public family::DiagnosticServices
{
public:
    unsigned calls = 0;
    void Call(GuestAddress target, GuestMemory& m,
        family::Registers& s) override
    {
        if (target != 0x82f99d98u)
            throw std::runtime_error("unexpected diagnostic boundary");
        ++calls;
        s.integer.r[3] = 0xfaceu;
        s.integer.r[9] = 0x1234u;
        m.WriteU32(Owner + 200u, 0xabcdu);
    }
};
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item);
    auto state = FromPpc(context);
    AllocationServices allocator; FreeServices free; DiagnosticServices diagnostic;
    const auto previous = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    diagnostic_calls = diagnostic_lower_calls = 0u;
    if (item.entry == 0x83058c60u)
        __imp__sub_83058C60(context, original.Bytes());
    else __imp__sub_83058CF0(context, original.Bytes());
    const auto original_csr = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    if (!family::Apply(item.entry, right, {{allocator, free}, diagnostic}, state))
        throw std::runtime_error("descriptor caller entry missing");
    const auto semantic_csr = simde_mm_getcsr();
    simde_mm_setcsr(previous);
    if (diagnostic_calls != unsigned(item.diagnostic) ||
        diagnostic_lower_calls != unsigned(item.diagnostic) ||
        diagnostic.calls != unsigned(item.diagnostic) ||
        allocator.pool != 0u || allocator.fp != 0u)
    {
        std::fprintf(stderr, "path %s diagnostic=%u lower=%u recovered=%u pool=%u fp=%u\n",
            item.name, diagnostic_calls, diagnostic_lower_calls,
            diagnostic.calls, allocator.pool, allocator.fp);
        throw std::runtime_error("descriptor caller fixture path");
    }
    const auto observed = FromPpc(context);
    if (!Same(observed, state) || original_csr != semantic_csr ||
        !original.EqualCommitted(recovered))
    {
        const auto& a = observed.integer; const auto& b = state.integer;
        std::fprintf(stderr,
            "mismatch %s entry=%08X r3=%016llX/%016llX SP=%016llX/%016llX LR=%016llX/%016llX CTR=%016llX/%016llX CA=%u/%u CR0=%u,%u,%u/%u,%u,%u CR6=%u,%u,%u/%u,%u,%u F0=%016llX/%016llX cache=%08X/%08X host=%08X/%08X\n",
            item.name, item.entry,
            static_cast<unsigned long long>(a.r[3]),
            static_cast<unsigned long long>(b.r[3]),
            static_cast<unsigned long long>(a.sp),
            static_cast<unsigned long long>(b.sp),
            static_cast<unsigned long long>(a.lr),
            static_cast<unsigned long long>(b.lr),
            static_cast<unsigned long long>(a.ctr),
            static_cast<unsigned long long>(b.ctr), a.xer_ca, b.xer_ca,
            a.cr0.lt, a.cr0.gt, a.cr0.eq, b.cr0.lt, b.cr0.gt, b.cr0.eq,
            a.cr6.lt, a.cr6.gt, a.cr6.eq, b.cr6.lt, b.cr6.gt, b.cr6.eq,
            static_cast<unsigned long long>(observed.f0_bits),
            static_cast<unsigned long long>(state.f0_bits),
            observed.cached_fp_control, state.cached_fp_control,
            original_csr, semantic_csr);
        for (unsigned i = 0; i < 32u; ++i)
            if (a.r[i] != b.r[i])
                std::fprintf(stderr, "r%u=%016llX/%016llX\n", i,
                    static_cast<unsigned long long>(a.r[i]),
                    static_cast<unsigned long long>(b.r[i]));
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

void OriginalSelector(PPCContext& c, std::uint8_t* b)
{ __imp__sub_82FAC238(c, b); }
void OriginalMutation(PPCContext& c, std::uint8_t* b)
{ __imp__sub_83056568(c, b); }
void OriginalAllocate(PPCContext& c, std::uint8_t* b)
{ __imp__sub_82FB36B0(c, b); }
void OriginalConstructor(PPCContext& c, std::uint8_t* b)
{ __imp__sub_83058590(c, b); }
void OriginalReturnRecord(PPCContext& c, std::uint8_t* b)
{ __imp__sub_82FAC980(c, b); }
void OriginalFill(PPCContext& c, std::uint8_t* b)
{ __imp__sub_82B7BC40(c, b); }
void OriginalDispatch(PPCContext& c, std::uint8_t* b)
{ __imp__sub_827C9DB0(c, b); }
void OriginalGeneral(PPCContext& c, std::uint8_t* b)
{ __imp__sub_827CA0E8(c, b); }
void OriginalHeapGeneral(PPCContext& c, std::uint8_t* b)
{ __imp__sub_827CAD80(c, b); }
void OriginalHeapGlobal(PPCContext& c, std::uint8_t* b)
{ __imp__sub_823ACC98(c, b); }
void OriginalPhysicalLower(PPCContext& c, std::uint8_t* b)
{ __imp__sub_827C9EB8(c, b); }
void OriginalFreeTail(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected heap release"); }
void OriginalSpecialFree(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected special release"); }
void OriginalPhysicalFree(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected physical release"); }
void OriginalNativePool(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("unexpected native pool"); }
void OriginalDiagnostic(PPCContext& c, std::uint8_t* b)
{ ++diagnostic_calls; __imp__sub_82F99F48(c, b); }
void OriginalDiagnosticLower(PPCContext& c, std::uint8_t* base)
{
    ++diagnostic_lower_calls;
    c.r3.u64 = 0xfaceu;
    c.r9.u64 = 0x1234u;
    PPC_STORE_U32(Owner + 200u, 0xabcdu);
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
void OriginalSave24(PPCContext& c, std::uint8_t* b)
{ OriginalSave(c, b, 24u); }
void OriginalRestore24(PPCContext& c, std::uint8_t* b)
{ OriginalRestore(c, b, 24u); }
void OriginalSave27(PPCContext& c, std::uint8_t* b)
{ OriginalSave(c, b, 27u); }
void OriginalRestore27(PPCContext& c, std::uint8_t* b)
{ OriginalRestore(c, b, 27u); }
void OriginalSave28(PPCContext& c, std::uint8_t* b)
{ OriginalSave(c, b, 28u); }
void OriginalRestore28(PPCContext& c, std::uint8_t* b)
{ OriginalRestore(c, b, 28u); }

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-array-callers %zu focused fixtures, %zu actual PPC comparisons\n",
            Cases.size(), Cases.size());
        std::puts("LIMIT diagnostic 82F99D98 is mutable selected boundary; pool exhaustion, faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
