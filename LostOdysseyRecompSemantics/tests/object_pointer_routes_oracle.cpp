#include "lo_semantics/object_pointer_routes.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_pointer_routes;
constexpr test::Region Regions[] = {{0u, 0x100000u}};
constexpr GuestAddress Object = 0x20000u, List = 0x30000u;
constexpr GuestAddress NodeA = 0x40000u, NodeB = 0x42000u;
constexpr GuestAddress TagA = 0x50000u, TagB = 0x52000u;
constexpr GuestAddress Result = 0x60000u;
constexpr GuestAddress Find = 0x822c6398u, Preferred = 0x822c66c8u;

enum class Mode { NullTarget, NegativeCount, MatchLater, Miss,
    NoPrimary, Direct, AliasedFallback };
struct Case { GuestAddress entry; Mode mode; GuestAddress expected; };
constexpr Case Cases[] = {
    {Find, Mode::NullTarget, 0u},
    {Find, Mode::NegativeCount, 0u},
    {Find, Mode::MatchLater, NodeB},
    {Find, Mode::Miss, 0u},
    {Preferred, Mode::NoPrimary, 0u},
    {Preferred, Mode::Direct, Result},
    {Preferred, Mode::AliasedFallback, Result}
};

family::Registers FromPpc(const PPCContext& context)
{
    return {context.r3.u64, context.r4.u64, context.r7.u64,
        context.r8.u64, context.r9.u64, context.r10.u64,
        context.r11.u64, context.xer.so,
        {context.cr6.lt, context.cr6.gt, context.cr6.eq, context.cr6.un}};
}

bool Same(const family::Registers& left, const family::Registers& right)
{
    return left.r3 == right.r3 && left.r4 == right.r4 &&
        left.r7 == right.r7 && left.r8 == right.r8 &&
        left.r9 == right.r9 && left.r10 == right.r10 &&
        left.r11 == right.r11 && left.xer_so == right.xer_so &&
        left.cr6 == right.cr6;
}

void Initialize(const Case& item, GuestMemory& memory)
{
    if (item.entry == Find)
    {
        memory.WriteU32(Object + 288u,
            item.mode == Mode::NegativeCount ? 0x80000000u : 2u);
        memory.WriteU32(Object + 284u, List);
        memory.WriteU32(List, NodeA);
        memory.WriteU32(List + 4u, NodeB);
        memory.WriteU32(NodeA + 60u, TagA);
        memory.WriteU32(NodeB + 60u, TagB);
    }
    else
    {
        memory.WriteU32(Object + 624u,
            item.mode == Mode::NoPrimary ? 0u :
            item.mode == Mode::AliasedFallback ? Object : NodeA);
        memory.WriteU32(Object + 888u,
            item.mode == Mode::Direct ? Result : 0u);
        memory.WriteU32(Object + 244u, Result);
        memory.WriteU32(NodeA + 244u, Result);
    }
}

void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    auto left = original.Memory(), right = recovered.Memory();
    Initialize(item, left); Initialize(item, right);

    PPCContext context{};
    context.r3.u64 = 0x1234567800020000ull;
    context.r4.u64 = 0x8877665500000000ull |
        (item.mode == Mode::NullTarget ? 0u :
            item.mode == Mode::Miss ? 0x53000u : TagB);
    context.r7.u64 = 0x1111222233334444ull;
    context.r8.u64 = 0x2222333344445555ull;
    context.r9.u64 = 0x3333444455556666ull;
    context.r10.u64 = 0x4444555566667777ull;
    context.r11.u64 = 0x5555666677778888ull;
    context.xer.so = 1;
    context.cr6.compare<int32_t>(-3, 2, context.xer);
    auto state = FromPpc(context);

    if (item.entry == Find)
        __imp__sub_822C6398(context, original.Bytes());
    else
        __imp__sub_822C66C8(context, original.Bytes());
    if (!family::Apply(item.entry, right, state))
        throw std::runtime_error("missing object pointer route");
    if (!Same(FromPpc(context), state) ||
        context.r3.u32 != item.expected ||
        state.r3 != item.expected ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("object pointer route original PPC comparison");
}
} // namespace

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS object-pointer-routes %zu original PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT selected GPR/CR6 and ordinary RAM; faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
