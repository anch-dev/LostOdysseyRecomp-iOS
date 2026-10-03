#include "lo_semantics/legacy_character_classification.h"
#include "semantic_oracle_support.h"

#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_character_classification;
constexpr test::Region Regions[] = {{0, 0x100000u}};
struct Case { GuestAddress entry; std::uint64_t input; bool accepted; };
constexpr Case Cases[] = {
    {0x82483ac8u, 'a', true}, {0x82483ac8u, 0x123456781234008cull, true},
    {0x82483ac8u, 0xd7u, true}, {0x82483ac8u, 0xbfu, false},
    {0x82483ac8u, 0x100u, false}, {0x82296938u, '5', true},
    {0x82296938u, '?', false}, {0x82296938u, 'Z', true}
};
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0); recovered.Fill(0);
    PPCContext context{};
    context.r1.u64 = 0x1234567800080000ull;
    context.lr = 0x99887766abcdef01ull;
    context.r3.u64 = item.input;
    context.r11.u64 = 0xffeeddccbbaa9988ull;
    context.r12.u64 = 0x1234432155667788ull;
    context.r31.u64 = 0x1122334455667788ull;
    context.xer.so = 1;
    family::Registers state{context.r1.u64, context.lr, context.r3.u64,
        context.r11.u64, context.r12.u64, context.r31.u64, 1, {}};
    if (item.entry == 0x82483ac8u)
        __imp__sub_82483AC8(context, original.Bytes());
    else
        __imp__sub_82296938(context, original.Bytes());
    auto memory = recovered.Memory();
    const bool applied = family::Apply(item.entry, memory, state);
    const family::Condition expected{context.cr6.lt, context.cr6.gt,
        context.cr6.eq, context.cr6.so};
    if (!applied || state.r3 != item.accepted || context.r3.u64 != state.r3 ||
        context.r1.u64 != state.sp || context.lr != state.lr ||
        context.r11.u64 != state.r11 || context.r12.u64 != state.r12 ||
        context.r31.u64 != state.r31 || expected != state.cr6 ||
        context.xer.so != state.xer_so || !original.EqualCommitted(recovered))
        throw std::runtime_error("legacy character class original PPC comparison");
}
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-character-classification %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT selected ABI and ordinary RAM; other inputs, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
