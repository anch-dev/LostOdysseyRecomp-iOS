#include "lo_semantics/float_triplet_transfer.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = float_triplet_transfer;
constexpr GuestAddress Object = 0x20000u;
constexpr test::Region Regions[] = {{0u, 0x50000u}};
struct Case
{
    GuestAddress entry, r4;
    std::array<std::uint32_t, 3> words;
    std::uint32_t control;
};
constexpr Case Cases[] = {
    {0x822c4d18u, 0x30000u, {0x3fc00000u, 0xc0100000u, 0x80000000u}, 0x9fc0u},
    {0x822c4d18u, Object + 76u, {0x3f800000u, 0x40000000u, 0xc0000000u}, 0x1f80u},
    {0x822c4dd0u, 0x40000u, {0x7f800000u, 0x40400000u, 0x80000000u}, 0x9fc0u},
    {0x822c4dd0u, Object + 1136u, {0xc0600000u, 0x3f800000u, 0x40000000u}, 0x1f80u}
};

struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Services final : family::NativeServices
{
    unsigned calls = 0;
    void SetHostFpControl(std::uint32_t control) override
    { ++calls; PPCFPSCRRegister{}.setcsr(control); }
};

void Check(const Case& item)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    auto left = original.Memory(), right = recovered.Memory();
    const auto source = item.entry == 0x822c4d18u ? item.r4 : Object + 1132u;
    for (std::uint32_t index = 0; index < 3u; ++index)
        for (auto* memory : {&left, &right})
            memory->WriteU32(source + index * 4u, item.words[index]);

    PPCContext context{};
    context.r3.u64 = 0x1122334400020000ull;
    context.r4.u64 = 0x8877665500000000ull | item.r4;
    context.r10.u64 = 0x1020304050607080ull;
    context.r11.u64 = 0x9988776655443322ull;
    context.lr = 0xaabbccddeeff0011ull;
    context.f0.u64 = 0x4009000000000000ull;
    context.f13.u64 = 0xc00a000000000000ull;
    context.fpscr.csr = item.control;
    const auto seeded = context;
    family::Registers state{context.r3.u64, context.r4.u64,
        context.r10.u64, context.r11.u64, context.lr,
        context.f0.u64, context.f13.u64, item.control};

    PPCFPSCRRegister{}.setcsr(item.control);
    switch (item.entry)
    {
    case 0x822c4d18u: __imp__sub_822C4D18(context, original.Bytes()); break;
    case 0x822c4dd0u: __imp__sub_822C4DD0(context, original.Bytes()); break;
    }
    const auto original_host_control = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(item.control);
    Services services;
    if (!family::Apply(item.entry, right, services, state))
        throw std::runtime_error("missing float-triplet entry");
    const auto recovered_host_control = PPCFPSCRRegister{}.getcsr();
    if (context.r3.u64 != state.r3 || context.r4.u64 != state.r4 ||
        context.r10.u64 != state.r10 || context.r11.u64 != state.r11 ||
        context.lr != state.lr || context.f0.u64 != state.f0_bits ||
        context.f13.u64 != state.f13_bits ||
        context.fpscr.csr != state.cached_fp_control ||
        context.r3.u64 != seeded.r3.u64 || context.r4.u64 != seeded.r4.u64 ||
        context.r10.u64 != seeded.r10.u64 || context.lr != seeded.lr ||
        context.f13.u64 != seeded.f13.u64 ||
        services.calls != unsigned((item.control & PPCFPSCRRegister::FlushMask) != 0) ||
        original_host_control != recovered_host_control ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("float-triplet original PPC comparison");
}
} // namespace

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS float-triplet-transfer %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT selected x64 FP cache/FPR/GPR, ordinary RAM and host flush mode; signaling NaNs, subnormals, host exceptions, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
