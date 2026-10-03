#include "lo_semantics/caller_frame_vtable_init.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = caller_frame_vtable_init;
constexpr test::Region Regions[] = {{0, 0x100000u}};
constexpr std::uint64_t InitialSp = 0x1234567800080000ull;
constexpr std::uint64_t InitialR31 = 0x99aabbccddeeff00ull;
struct Case
{
    GuestAddress entry;
    std::uint64_t r3 = 0x1122334400060000ull;
    std::uint64_t caller_frame = 0x5566778800070000ull;
};
constexpr Case Cases[] = {
    {0x828138f8u},
    {0x828138f8u, InitialSp + 20u},
    {0x82373104u},
    {0x82373104u, 0x1122334400060000ull, 0x50u},
    {0x8237dafcu},
    {0x8237dafcu, 0x1122334400060000ull, 0xffffffff00070000ull}
};

void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0); recovered.Fill(0);
    auto left = original.Memory(), right = recovered.Memory();
    PPCContext context{};
    context.r1.u64 = InitialSp;
    context.lr = 0x99887766abcdef01ull;
    context.r3.u64 = item.r3;
    context.r11.u64 = 0x7766554433221100ull;
    context.r12.u64 = item.caller_frame;
    context.r31.u64 = InitialR31;
    family::Registers state{context.r1.u64, context.lr, context.r3.u64,
        context.r11.u64, context.r12.u64, context.r31.u64};

    switch (item.entry)
    {
    case 0x828138f8u: __imp__sub_828138F8(context, original.Bytes()); break;
    case 0x82373104u: __imp__sub_82373104(context, original.Bytes()); break;
    case 0x8237dafcu: __imp__sub_8237DAFC(context, original.Bytes()); break;
    }

    const bool lower = item.entry == 0x828138f8u;
    const auto frame_size = item.entry == 0x82373104u ? 160u : 144u;
    const auto member_offset = item.entry == 0x82373104u ? 104u : 80u;
    const auto object = lower ? item.r3 : item.caller_frame - frame_size + member_offset;
    const auto lower_sp = lower ? InitialSp : InitialSp - 96u;
    const auto saved_r31 = lower ? InitialR31 : item.caller_frame - frame_size;
    const auto spill = static_cast<GuestAddress>(lower_sp + 20u);
    const auto expected_spill = static_cast<GuestAddress>(object) == spill
        ? 0x8208018cu : static_cast<GuestAddress>(object);
    if (left.ReadU32(static_cast<GuestAddress>(object)) != 0x8208018cu ||
        recovery_abi::ReadU64(left, static_cast<GuestAddress>(lower_sp - 8u)) != saved_r31 ||
        left.ReadU32(static_cast<GuestAddress>(lower_sp - 16u)) !=
            static_cast<GuestAddress>(lower_sp) ||
        left.ReadU32(spill) != expected_spill ||
        !family::Apply(item.entry, right, state) ||
        context.r1.u64 != state.sp || context.lr != state.lr ||
        context.r3.u64 != state.r3 || context.r11.u64 != state.r11 ||
        context.r12.u64 != state.r12 || context.r31.u64 != state.r31 ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("caller frame vtable init original PPC comparison");
}
}

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS caller-frame-vtable-init %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT selected ABI/RAM; faults, MMIO, concurrency and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
