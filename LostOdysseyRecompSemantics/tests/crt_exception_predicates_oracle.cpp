#include "lo_semantics/crt_exception_predicates.h"
#include "semantic_oracle_support.h"

#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = crt_exception_predicates;
constexpr test::Region Regions[] = {{0, 0x100000u}};
struct Case { GuestAddress entry; std::uint32_t value; };
constexpr Case Cases[] = {
    {0x82b80480u, 0}, {0x82b80480u, 0x81u},
    {0x82b8223cu, 0xc0000017u}, {0x82b8223cu, 0xc0000018u}
};

void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0); recovered.Fill(0);
    auto left = original.Memory(), right = recovered.Memory();
    for (auto* memory : {&left, &right})
    {
        memory->WriteU8(0x7001fu, static_cast<std::uint8_t>(item.value));
        memory->WriteU32(0x90000u, 0x91000u);
        memory->WriteU32(0x91000u, item.value);
    }
    PPCContext context{};
    context.r1.u64 = 0x1234567800080000ull;
    context.r3.u64 = 0x1122334400090000ull;
    context.r8.u64 = 0xaabbccdd12345678ull;
    context.r10.u64 = 0x8877665544332211ull;
    context.r11.u64 = 0xffeeddccbbaa9988ull;
    context.r12.u64 = 0x5566778800070000ull;
    context.r31.u64 = 0x9988776655443322ull;
    family::Registers state{context.r1.u64, context.r3.u64, context.r8.u64,
        context.r10.u64, context.r11.u64, context.r12.u64, context.r31.u64};
    if (item.entry == 0x82b80480u)
        __imp__sub_82B80480(context, original.Bytes());
    else
        __imp__sub_82B8223C(context, original.Bytes());
    if (!family::Apply(item.entry, right, state) ||
        state.r3 != (item.entry == 0x82b80480u ? (item.value != 0u) :
            (item.value == 0xc0000017u)) ||
        context.r1.u64 != state.sp || context.r3.u64 != state.r3 ||
        context.r8.u64 != state.r8 || context.r10.u64 != state.r10 ||
        context.r11.u64 != state.r11 || context.r12.u64 != state.r12 ||
        context.r31.u64 != state.r31 || !original.EqualCommitted(recovered))
        throw std::runtime_error("CRT exception predicate original PPC comparison");
}
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS crt-exception-predicates %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT ordinary RAM and selected GPRs; faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
