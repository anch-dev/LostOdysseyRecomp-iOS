#include "lo_semantics/legacy_fp_flagged_routes.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_fp_flagged_routes;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<test::Region, 3> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x8000u},
    {0x8210a000u, 0x2000u}
}};
constexpr std::array<GuestAddress, 4> Entries{{
    0x83053c10u, 0x83053c78u, 0x82b7def0u, 0x82b7df28u
}};
struct Case
{
    const char* name;
    std::uint64_t bits;
    std::uint32_t flags, mode;
};
constexpr std::array<Case, 8> Cases{{
    {"no-flags-negative-zero", 0x8000000000000000ull, 0u, 2u},
    {"round-and-clamp", 0x403419999999999aull, 1u, 2u},
    {"copy-positive-sign", 0x400c000000000000ull, 2u, 2u},
    {"mode-two-subtract", 0x4002000000000000ull, 4u, 2u},
    {"other-mode-flip", 0xc002000000000000ull, 4u, 3u},
    {"clamp-then-copy", 0xc034400000000000ull, 3u, 2u},
    {"all-flags-subtract", 0x403419999999999aull, 7u, 2u},
    {"all-flags-flip", 0xc002000000000000ull, 7u, 3u}
}};
unsigned numeric_calls = 0, copy_calls = 0, flip_calls = 0;
unsigned category_calls = 0, special_calls = 0;

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
        state.numeric.classifier.integer.r[index] =
            index == 1u ? 0u : fields[index]->u64;
    auto& integer = state.numeric.classifier.integer;
    integer.sp = c.r1.u64; integer.lr = c.lr; integer.ctr = c.ctr.u64;
    integer.xer_so = c.xer.so; integer.xer_ca = c.xer.ca;
    integer.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    integer.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.numeric.classifier.f0_bits = c.f0.u64;
    state.numeric.classifier.f1_bits = c.f1.u64;
    state.numeric.classifier.cached_fp_control = c.fpscr.csr;
    state.numeric.f31_bits = c.f31.u64;
    state.f2_bits = c.f2.u64;
    return state;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{
    return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so;
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.numeric.classifier.integer;
    const auto& y = b.numeric.classifier.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.numeric.classifier.f0_bits == b.numeric.classifier.f0_bits &&
        a.numeric.classifier.f1_bits == b.numeric.classifier.f1_bits &&
        a.numeric.classifier.cached_fp_control ==
            b.numeric.classifier.cached_fp_control &&
        a.numeric.f31_bits == b.numeric.f31_bits &&
        a.f2_bits == b.f2_bits;
}
PPCContext Initial(const Case& item, GuestAddress entry)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r4.u64 = entry == 0x83053c10u ? item.flags : item.mode;
    c.r5.u64 = item.flags;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    c.f0.u64 = 0xc00a000000000000ull;
    c.f1.u64 = item.bits;
    c.f2.u64 = 0x8000000000000000ull;
    c.f31.u64 = 0x400a000000000000ull;
    c.fpscr.csr = 0x9fc0u;
    return c;
}
void Seed(GuestMemory& memory)
{
    memory.WriteU32(0x82000e50u, 0xc1200000u); // -10.0f
    memory.WriteU32(0x82007784u, 0x41200000u); // +10.0f
    WriteU64(memory, 0x82000f28u, 0x3ff0000000000000ull);
    WriteU64(memory, 0x82000fe8u, 0u);
    WriteU64(memory, 0x8210aac8u, 0x3ff0000000000000ull);
}
struct Services final : legacy_fp_classification::HostFpServices
{
    unsigned calls = 0;
    void SetHostFpControl(std::uint32_t control) override
    { ++calls; PPCFPSCRRegister{}.setcsr(control); }
};
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
void CallOriginal(GuestAddress entry, PPCContext& c, std::uint8_t* base)
{
    switch (entry)
    {
    case 0x83053c10u: __imp__sub_83053C10(c, base); return;
    case 0x83053c78u: __imp__sub_83053C78(c, base); return;
    case 0x82b7def0u: __imp__sub_82B7DEF0(c, base); return;
    case 0x82b7df28u: __imp__sub_82B7DF28(c, base); return;
    default: throw std::runtime_error("unknown original FP flagged route");
    }
}
void Check(const Case& item, GuestAddress entry)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left); Seed(right);
    auto context = Initial(item, entry);
    auto state = FromPpc(context);
    numeric_calls = copy_calls = flip_calls = 0;
    category_calls = special_calls = 0;
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    CallOriginal(entry, context, original.Bytes());
    const auto original_control = PPCFPSCRRegister{}.getcsr();
    Services services;
    PPCFPSCRRegister{}.setcsr(state.numeric.classifier.cached_fp_control);
    if (!family::Apply(entry, right, services, state))
        throw std::runtime_error("recovered FP flagged route missing");
    const auto recovered_control = PPCFPSCRRegister{}.getcsr();
    const auto observed = FromPpc(context);
    const bool first = entry == 0x83053c10u;
    const bool second = entry == 0x83053c78u;
    const unsigned expected_numeric = first || second ? item.flags & 1u : 0u;
    const unsigned expected_copy = first || second ?
        (item.flags >> 1u) & 1u : 0u;
    const unsigned expected_flip = second && (item.flags & 4u) &&
        item.mode != 2u ? 1u : 0u;
    const unsigned expected_services = (first || second) &&
        item.flags == 0u ? 0u : 1u;
    if (numeric_calls != expected_numeric ||
        copy_calls != expected_copy || flip_calls != expected_flip ||
        category_calls != expected_numeric || special_calls != 0u ||
        services.calls != expected_services)
        throw std::runtime_error("original FP flagged fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered) ||
        original_control != recovered_control)
    {
        std::fprintf(stderr, "mismatch %s entry=%08X F0=%016llX/%016llX F1=%016llX/%016llX F2=%016llX/%016llX CSR=%08X/%08X\n",
            item.name, entry,
            static_cast<unsigned long long>(observed.numeric.classifier.f0_bits),
            static_cast<unsigned long long>(state.numeric.classifier.f0_bits),
            static_cast<unsigned long long>(observed.numeric.classifier.f1_bits),
            static_cast<unsigned long long>(state.numeric.classifier.f1_bits),
            static_cast<unsigned long long>(observed.f2_bits),
            static_cast<unsigned long long>(state.f2_bits),
            original_control, recovered_control);
        throw std::runtime_error(item.name);
    }
}
} // namespace

void OriginalNumeric(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053c30u && c.lr != 0x83053ca4u)
        throw std::runtime_error("actual 83053360 call LR");
    __imp__sub_83053360(c, base);
    ++numeric_calls;
}
void OriginalCopySign(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053c44u && c.lr != 0x83053cb8u)
        throw std::runtime_error("actual 82B7DEF0 call LR");
    __imp__sub_82B7DEF0(c, base);
    ++copy_calls;
}
void OriginalFlipSign(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053cdcu)
        throw std::runtime_error("actual 82B7DF28 call LR");
    __imp__sub_82B7DF28(c, base);
    ++flip_calls;
}
void OriginalCategory(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053378u)
        throw std::runtime_error("actual 82B7DFC0 call LR");
    __imp__sub_82B7DFC0(c, base);
    ++category_calls;
}
void OriginalSpecial(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x82b7dfe4u)
        throw std::runtime_error("actual 82B82340 call LR");
    __imp__sub_82B82340(c, base);
    ++special_calls;
}

int main()
{
    try
    {
        for (const auto& item : Cases)
            for (const auto entry : Entries) Check(item, entry);
        std::printf("PASS legacy-fp-flagged-routes %zu focused inputs, %zu actual PPC entry comparisons\n",
            Cases.size(), Cases.size() * Entries.size());
        std::puts("LIMIT selected finite and signed-zero ordinary-RAM paths; NaN payload, faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
