#include "lo_semantics/legacy_fp_classification.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_fp_classification;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x80000u;
constexpr GuestAddress Zero = 0x82000fe8u;
constexpr std::array<test::Region, 2> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x2000u}
}};
constexpr std::array<GuestAddress, 4> Entries{{
    0x82b7df58u, 0x82b7df78u, 0x82b7dfc0u, 0x82b82340u
}};

struct Case
{
    const char* name;
    std::uint64_t bits;
    std::uint32_t expected_finite, expected_nan, expected_special;
    std::uint32_t expected_category_low;
    unsigned special_calls;
};
constexpr std::array<Case, 8> Cases{{
    {"finite-positive", 0x400c000000000000ull, 1u, 0u, 0u, 256u, 0u},
    {"negative-subnormal", 0x8000000000000001ull, 1u, 0u, 0u,
        16u, 0u},
    {"positive-zero", 0x0000000000000000ull, 1u, 0u, 0u, 64u, 0u},
    {"negative-zero", 0x8000000000000000ull, 1u, 0u, 0u,
        32u, 0u},
    {"positive-infinity", 0x7ff0000000000000ull, 0u, 0u, 1u,
        512u, 1u},
    {"negative-infinity", 0xfff0000000000000ull, 0u, 0u, 2u,
        4u, 1u},
    {"negative-quiet-nan-payload", 0xfff8000000000042ull,
        0u, 1u, 3u, 2u, 1u},
    {"signaling-nan-payload", 0x7ff0000000000042ull,
        0u, 1u, 4u, 1u, 1u}
}};
unsigned original_special_calls = 0;

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
        state.integer.r[index] = index == 1u ? 0u : fields[index]->u64;
    auto& integer = state.integer;
    integer.sp = c.r1.u64;
    integer.lr = c.lr; integer.ctr = c.ctr.u64;
    integer.xer_so = c.xer.so; integer.xer_ca = c.xer.ca;
    integer.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    integer.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.f0_bits = c.f0.u64; state.f1_bits = c.f1.u64;
    state.cached_fp_control = c.fpscr.csr;
    return state;
}
bool Same(const family::Registers& left, const family::Registers& right)
{
    const auto& a = left.integer;
    const auto& b = right.integer;
    return a.r == b.r && a.sp == b.sp && a.lr == b.lr &&
        a.ctr == b.ctr && a.xer_so == b.xer_so && a.xer_ca == b.xer_ca &&
        a.cr0.lt == b.cr0.lt && a.cr0.gt == b.cr0.gt &&
        a.cr0.eq == b.cr0.eq && a.cr0.so == b.cr0.so &&
        a.cr6.lt == b.cr6.lt && a.cr6.gt == b.cr6.gt &&
        a.cr6.eq == b.cr6.eq && a.cr6.so == b.cr6.so &&
        left.f0_bits == right.f0_bits && left.f1_bits == right.f1_bits &&
        left.cached_fp_control == right.cached_fp_control;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned index = 0; index < 32u; ++index)
        fields[index]->u64 = 0x1122334400000000ull + index;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1;
    c.xer.so = 1; c.xer.ca = 1;
    c.f0.u64 = 0xc00a000000000000ull;
    c.f1.u64 = item.bits;
    c.fpscr.csr = 0x9fc0u;
    return c;
}
struct Services final : family::HostFpServices
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
    case 0x82b7df58u: __imp__sub_82B7DF58(c, base); return;
    case 0x82b7df78u: __imp__sub_82B7DF78(c, base); return;
    case 0x82b7dfc0u: __imp__sub_82B7DFC0(c, base); return;
    case 0x82b82340u: __imp__sub_82B82340(c, base); return;
    default: throw std::runtime_error("unknown original FP classifier");
    }
}
std::uint32_t Expected(const Case& item, GuestAddress entry)
{
    switch (entry)
    {
    case 0x82b7df58u: return item.expected_finite;
    case 0x82b7df78u: return item.expected_nan;
    case 0x82b7dfc0u: return item.expected_category_low;
    case 0x82b82340u: return item.expected_special;
    default: throw std::runtime_error("unknown expected FP classifier");
    }
}
void Check(const Case& item, GuestAddress entry)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    WriteU64(left, Zero, 0u); WriteU64(right, Zero, 0u);
    auto context = Initial(item);
    auto state = FromPpc(context);
    original_special_calls = 0;
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    CallOriginal(entry, context, original.Bytes());
    const auto original_control = PPCFPSCRRegister{}.getcsr();
    Services services;
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(entry, right, services, state))
        throw std::runtime_error("recovered FP classifier missing");
    const auto recovered_control = PPCFPSCRRegister{}.getcsr();
    const auto observed = FromPpc(context);
    const unsigned expected_special_calls =
        entry == 0x82b7dfc0u ? item.special_calls : 0u;
    if (static_cast<std::uint32_t>(observed.integer.r[3]) !=
            Expected(item, entry) ||
        original_special_calls != expected_special_calls)
        throw std::runtime_error("original FP classifier fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered) ||
        original_control != recovered_control || services.calls != 1u)
        throw std::runtime_error(item.name);
}
} // namespace

void OriginalSpecial(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x82b7dfe4u)
        throw std::runtime_error("actual 82B82340 call LR");
    __imp__sub_82B82340(c, base);
    ++original_special_calls;
}

int main()
{
    try
    {
        for (const auto& item : Cases)
            for (const auto entry : Entries)
                Check(item, entry);
        std::printf("PASS legacy-fp-classification %zu input cases, %zu original PPC entry comparisons\n",
            Cases.size(), Cases.size() * Entries.size());
        std::puts("LIMIT selected ordinary RAM binary64 payloads and cached FP control; faults/MMIO/runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
