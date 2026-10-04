#include "lo_semantics/legacy_fp_numeric_routes.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_fp_numeric_routes;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x80000u;
constexpr std::array<test::Region, 2> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x8000u}
}};
constexpr std::array<GuestAddress, 3> Entries{{
    0x83053360u, 0x83053610u, 0x82ff99e0u
}};
struct Case
{
    const char* name;
    std::uint64_t bits;
    unsigned sqrt_category, sqrt_normalize, special;
};
constexpr std::array<Case, 9> Cases{{
    {"positive-zero", 0x0000000000000000ull, 1u, 1u, 0u},
    {"negative-zero", 0x8000000000000000ull, 1u, 1u, 0u},
    {"one-early-return", 0x3ff0000000000000ull, 0u, 0u, 0u},
    {"four-sqrt", 0x4010000000000000ull, 1u, 1u, 0u},
    {"negative-four", 0xc010000000000000ull, 0u, 0u, 0u},
    {"positive-infinity", 0x7ff0000000000000ull, 1u, 0u, 1u},
    {"quiet-nan-payload", 0x7ff8000000000042ull, 1u, 0u, 1u},
    {"signaling-nan-payload", 0x7ff0000000000042ull, 1u, 0u, 1u},
    {"smallest-subnormal", 0x0000000000000001ull, 1u, 1u, 0u}
}};
unsigned category_calls = 0, normalize_calls = 0, special_calls = 0;

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
        state.classifier.integer.r[index] =
            index == 1u ? 0u : fields[index]->u64;
    auto& integer = state.classifier.integer;
    integer.sp = c.r1.u64; integer.lr = c.lr; integer.ctr = c.ctr.u64;
    integer.xer_so = c.xer.so; integer.xer_ca = c.xer.ca;
    integer.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    integer.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    state.classifier.f0_bits = c.f0.u64;
    state.classifier.f1_bits = c.f1.u64;
    state.classifier.cached_fp_control = c.fpscr.csr;
    state.f31_bits = c.f31.u64;
    return state;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{
    return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so;
}
bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.classifier.integer;
    const auto& y = b.classifier.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.classifier.f0_bits == b.classifier.f0_bits &&
        a.classifier.f1_bits == b.classifier.f1_bits &&
        a.classifier.cached_fp_control == b.classifier.cached_fp_control &&
        a.f31_bits == b.f31_bits;
}
void Diagnose(const Case& item, GuestAddress entry,
    const family::Registers& original,
    const family::Registers& recovered, const PPCContext& context,
    const test::GuestWindow& left, const test::GuestWindow& right,
    std::uint32_t original_control, std::uint32_t recovered_control,
    unsigned service_calls)
{
    const auto& a = original.classifier.integer;
    const auto& b = recovered.classifier.integer;
    std::fprintf(stderr, "mismatch %s entry=%08X hostCSR=%08X/%08X services=%u\n",
        item.name, entry, original_control, recovered_control,
        service_calls);
    std::fprintf(stderr, "F0=%016llX/%016llX F1=%016llX/%016llX F31=%016llX/%016llX originalF13=%016llX\n",
        static_cast<unsigned long long>(original.classifier.f0_bits),
        static_cast<unsigned long long>(recovered.classifier.f0_bits),
        static_cast<unsigned long long>(original.classifier.f1_bits),
        static_cast<unsigned long long>(recovered.classifier.f1_bits),
        static_cast<unsigned long long>(original.f31_bits),
        static_cast<unsigned long long>(recovered.f31_bits),
        static_cast<unsigned long long>(context.f13.u64));
    std::fprintf(stderr, "SP=%016llX/%016llX LR=%016llX/%016llX CTR=%016llX/%016llX CA=%u/%u SO=%u/%u cache=%08X/%08X\n",
        static_cast<unsigned long long>(a.sp),
        static_cast<unsigned long long>(b.sp),
        static_cast<unsigned long long>(a.lr),
        static_cast<unsigned long long>(b.lr),
        static_cast<unsigned long long>(a.ctr),
        static_cast<unsigned long long>(b.ctr),
        a.xer_ca, b.xer_ca, a.xer_so, b.xer_so,
        original.classifier.cached_fp_control,
        recovered.classifier.cached_fp_control);
    std::fprintf(stderr, "CR0=%u%u%u%u/%u%u%u%u CR6=%u%u%u%u/%u%u%u%u\n",
        a.cr0.lt, a.cr0.gt, a.cr0.eq, a.cr0.so,
        b.cr0.lt, b.cr0.gt, b.cr0.eq, b.cr0.so,
        a.cr6.lt, a.cr6.gt, a.cr6.eq, a.cr6.so,
        b.cr6.lt, b.cr6.gt, b.cr6.eq, b.cr6.so);
    for (unsigned index = 0; index < a.r.size(); ++index)
        if (a.r[index] != b.r[index])
            std::fprintf(stderr, "r%u=%016llX/%016llX\n", index,
                static_cast<unsigned long long>(a.r[index]),
                static_cast<unsigned long long>(b.r[index]));
    for (const auto region : Regions)
        for (std::size_t offset = 0; offset < region.size; ++offset)
        {
            const auto address = region.base + offset;
            if (left.Bytes()[address] != right.Bytes()[address])
            {
                std::fprintf(stderr, "RAM first difference %08zX=%02X/%02X\n",
                    address, left.Bytes()[address], right.Bytes()[address]);
                return;
            }
        }
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
    c.f31.u64 = 0x400a000000000000ull;
    c.fpscr.csr = 0x9fc0u;
    return c;
}
void Seed(GuestMemory& memory)
{
    memory.WriteU32(0x82000e50u, 0xc1200000u); // -10.0f
    memory.WriteU32(0x82007784u, 0x41200000u); // +10.0f
    WriteU64(memory, 0x82000f28u, 0x3ff0000000000000ull); // 1.0
    WriteU64(memory, 0x82000fe8u, 0u); // +0.0
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
    case 0x83053360u: __imp__sub_83053360(c, base); return;
    case 0x83053610u: __imp__sub_83053610(c, base); return;
    case 0x82ff99e0u: __imp__sub_82FF99E0(c, base); return;
    default: throw std::runtime_error("unknown original FP numeric route");
    }
}
void Check(const Case& item, GuestAddress entry)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xa5); recovered.Fill(0xa5);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left); Seed(right);
    auto context = Initial(item);
    auto state = FromPpc(context);
    category_calls = normalize_calls = special_calls = 0;
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    CallOriginal(entry, context, original.Bytes());
    const auto original_control = PPCFPSCRRegister{}.getcsr();
    Services services;
    PPCFPSCRRegister{}.setcsr(state.classifier.cached_fp_control);
    if (!family::Apply(entry, right, services, state))
        throw std::runtime_error("recovered FP numeric route missing");
    const auto recovered_control = PPCFPSCRRegister{}.getcsr();
    const auto observed = FromPpc(context);
    if ((entry == 0x83053360u &&
            (category_calls != 1u || special_calls != item.special ||
                normalize_calls != 0u)) ||
        (entry == 0x82ff99e0u &&
            (category_calls != 0u || normalize_calls != 0u ||
                special_calls != 0u)) ||
        (entry == 0x83053610u &&
            (category_calls != item.sqrt_category ||
                normalize_calls != item.sqrt_normalize ||
                special_calls != item.special)))
        throw std::runtime_error("original FP numeric fixture path");
    if (!Same(observed, state) || !original.EqualCommitted(recovered) ||
        original_control != recovered_control || services.calls != 1u)
    {
        Diagnose(item, entry, observed, state, context, original,
            recovered, original_control, recovered_control,
            services.calls);
        throw std::runtime_error(item.name);
    }
}
} // namespace

void OriginalCategory(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x83053378u && c.lr != 0x8305365cu)
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
void OriginalNormalize(PPCContext& c, std::uint8_t* base)
{
    if (c.lr != 0x8305368cu)
        throw std::runtime_error("actual 82FF99E0 call LR");
    __imp__sub_82FF99E0(c, base);
    ++normalize_calls;
}

int main()
{
    try
    {
        for (const auto& item : Cases)
            for (const auto entry : Entries) Check(item, entry);
        std::printf("PASS legacy-fp-numeric-routes %zu input cases, %zu actual PPC entry comparisons\n",
            Cases.size(), Cases.size() * Entries.size());
        std::puts("LIMIT selected ordinary RAM and FP-control paths; host exception modes, faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
