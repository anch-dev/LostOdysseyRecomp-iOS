#include "lo_semantics/crt_float_sign.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_float_sign;
constexpr GuestAddress Value = 0x20000u, Reference = 0x82000fe8u;
constexpr test::Region Regions[] = {{0, 0x30000u}, {0x82000000u, 0x1000u}};
struct Case { std::uint64_t value, reference; std::uint32_t control; };
constexpr Case Cases[] = {
    {0xbff0000000000000ull, 0, 0x9fc0u},
    {0x8000000000000000ull, 0, 0x1f80u},
    {0x4000000000000000ull, 0, 0x9fc0u},
    {0x7ff8000000000123ull, 0, 0x9fc0u},
    {0x3ff0000000000000ull, 0x4000000000000000ull, 0x1f80u}
};
struct Services final : family::NativeServices
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
void Check(const Case& item)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0); recovered.Fill(0);
    auto left = original.Memory(), right = recovered.Memory();
    for (auto* memory : {&left, &right})
    {
        recovery_abi::WriteU64(*memory, Value, item.value);
        recovery_abi::WriteU64(*memory, Reference, item.reference);
    }
    PPCContext context{};
    context.r3.u64 = 0x1122334400020000ull;
    context.r11.u64 = 0x8877665544332211ull;
    context.fpscr.csr = item.control;
    context.f0.u64 = 0x99aabbccddeeff00ull;
    context.f13.u64 = 0x123456789abcdef0ull;
    context.cr6 = {1, 1, 0, {0}};
    family::Registers state{context.r3.u64, context.r11.u64,
        context.f0.u64, context.f13.u64, item.control, {1, 1, 0, 0}};
    PPCFPSCRRegister{}.setcsr(item.control);
    __imp__sub_82B7EFB0(context, original.Bytes());
    PPCFPSCRRegister{}.setcsr(item.control);
    Services services;
    if (!family::Apply(0x82b7efb0u, right, services, state))
        throw std::runtime_error("missing float-sign entry");
    const family::Comparison expected{context.cr6.lt, context.cr6.gt,
        context.cr6.eq, context.cr6.un};
    if (context.r3.u64 != state.r3 || context.r11.u64 != state.r11 ||
        context.f0.u64 != state.f0_bits || context.f13.u64 != state.f13_bits ||
        context.fpscr.csr != state.cached_fp_control || expected != state.cr6 ||
        services.calls != unsigned((item.control & 0x8040u) != 0) ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("float-sign original PPC comparison");
}
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS crt-float-sign %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT generated x64 FP control cache and ordinary RAM; signaling NaN, subnormal, host exceptions, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
