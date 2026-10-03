#include "lo_semantics/caller_frame_vtable_restore.h"
#include "lo_semantics/pointer_fields.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = caller_frame_vtable_restore;
constexpr test::Region Regions[] = {{0, 0x100000u}};
constexpr GuestAddress FrameMember = 0x70000u - 512u + 88u;
struct Case { GuestAddress entry, indirect_target; };
constexpr Case Cases[] = {{0x82290668u, 0x60000u}, {0x82290690u, 0x60100u},
    {0x822906e0u, 0x60200u}, {0x82290668u, 0x60300u}};
GuestMemory* active = nullptr;
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0); recovered.Fill(0);
    auto left = original.Memory(), right = recovered.Memory();
    left.WriteU32(FrameMember, item.indirect_target);
    right.WriteU32(FrameMember, item.indirect_target);
    PPCContext context{};
    context.r1.u64 = 0x1234567800080000ull;
    context.lr = 0x99887766abcdef01ull;
    context.r3.u64 = 0x1122334400020000ull;
    context.r11.u64 = 0x7766554433221100ull;
    context.r12.u64 = 0x5566778800070000ull;
    context.r31.u64 = 0x99aabbccddeeff00ull;
    family::Registers state{context.r1.u64, context.lr, context.r3.u64,
        context.r11.u64, context.r12.u64, context.r31.u64};
    active = &left;
    switch (item.entry)
    {
    case 0x82290668u: __imp__sub_82290668(context, original.Bytes()); break;
    case 0x82290690u: __imp__sub_82290690(context, original.Bytes()); break;
    case 0x822906e0u: __imp__sub_822906E0(context, original.Bytes()); break;
    }
    active = nullptr;
    const auto object = item.entry == 0x82290668u ? item.indirect_target : FrameMember;
    if (left.ReadU32(object) != 0x8218018cu ||
        !family::Apply(item.entry, right, state) || context.r1.u64 != state.sp ||
        context.lr != state.lr || context.r3.u64 != state.r3 ||
        context.r11.u64 != state.r11 || context.r12.u64 != state.r12 ||
        context.r31.u64 != state.r31 || !original.EqualCommitted(recovered))
        throw std::runtime_error("caller frame vtable original PPC comparison");
}
}
void OriginalDefaultVTable(PPCContext& context, std::uint8_t*)
{
    constexpr std::array<ConstantFieldAssignment, 2> assignments{{
        {PointerFieldRegister::R11, 0xffffffff82180000ull},
        {PointerFieldRegister::R11, 0xffffffff8218018cull}}};
    constexpr std::array<ConstantFieldWrite, 1> writes{{
        {0, PointerFieldWidth::Word, 0x8218018cu}}};
    PointerFieldRegisters call{};
    call.r3 = context.r3.u64; call.r11 = context.r11.u64;
    InitializeConstantFields(*active, call, PointerFieldRegister::R3, assignments, writes);
    context.r3.u64 = call.r3; context.r11.u64 = call.r11;
}
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS caller-frame-vtable-restore %zu original PPC cases\n", std::size(Cases));
        std::puts("LIMIT selected ABI/RAM; accepted lower field helper reused, faults, MMIO and runtime remain open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
