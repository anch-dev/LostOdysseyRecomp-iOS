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
struct Case {
    GuestAddress entry, indirect_target;
    std::uint32_t frame_size = 512u, member_offset = 88u;
    std::uint64_t caller_frame = 0x5566778800070000ull;
};
#ifdef LO_CALLER_FRAME_VTABLE_PPC9_ONLY
constexpr Case Cases[] = {
    {0x823730b4u, 0x60400u, 160u, 80u},
    {0x823730dcu, 0x60500u, 160u, 104u},
    {0x8237312cu, 0x60600u, 160u, 104u},
    {0x8237daacu, 0x60700u, 144u, 80u},
    {0x8237dad4u, 0x60800u, 144u, 80u},
    {0x8237db24u, 0x60900u, 144u, 80u},
    {0x823730dcu, 0x60a00u, 160u, 104u, 0x50u},
    {0x8237dad4u, 0x60b00u, 144u, 80u, 0xffffffff00070000ull}
};
#else
constexpr Case Cases[] = {{0x82290668u, 0x60000u}, {0x82290690u, 0x60100u},
    {0x822906e0u, 0x60200u}, {0x82290668u, 0x60300u}};
#endif
GuestMemory* active = nullptr;
#ifdef LO_CALLER_FRAME_VTABLE_PPC9_ONLY
std::uint64_t expected_object = 0, expected_sp = 0;
GuestAddress expected_return = 0;
unsigned observed_calls = 0;
#endif
void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0); recovered.Fill(0);
    auto left = original.Memory(), right = recovered.Memory();
    const auto member = item.caller_frame - item.frame_size + item.member_offset;
    const auto member_address = static_cast<GuestAddress>(member);
    left.WriteU32(member_address, item.indirect_target);
    right.WriteU32(member_address, item.indirect_target);
    PPCContext context{};
    context.r1.u64 = 0x1234567800080000ull;
    context.lr = 0x99887766abcdef01ull;
    context.r3.u64 = 0x1122334400020000ull;
    context.r11.u64 = 0x7766554433221100ull;
    context.r12.u64 = item.caller_frame;
    context.r31.u64 = 0x99aabbccddeeff00ull;
    family::Registers state{context.r1.u64, context.lr, context.r3.u64,
        context.r11.u64, context.r12.u64, context.r31.u64};
    const bool indirect = item.entry == 0x82290668u ||
        item.entry == 0x823730b4u || item.entry == 0x8237daacu;
#ifdef LO_CALLER_FRAME_VTABLE_PPC9_ONLY
    expected_object = indirect ? item.indirect_target : member;
    expected_sp = context.r1.u64 - 96u;
    expected_return = item.entry + 0x18u;
    observed_calls = 0;
#endif
    active = &left;
    switch (item.entry)
    {
#ifdef LO_CALLER_FRAME_VTABLE_PPC9_ONLY
    case 0x823730b4u: __imp__sub_823730B4(context, original.Bytes()); break;
    case 0x823730dcu: __imp__sub_823730DC(context, original.Bytes()); break;
    case 0x8237312cu: __imp__sub_8237312C(context, original.Bytes()); break;
    case 0x8237daacu: __imp__sub_8237DAAC(context, original.Bytes()); break;
    case 0x8237dad4u: __imp__sub_8237DAD4(context, original.Bytes()); break;
    case 0x8237db24u: __imp__sub_8237DB24(context, original.Bytes()); break;
#else
    case 0x82290668u: __imp__sub_82290668(context, original.Bytes()); break;
    case 0x82290690u: __imp__sub_82290690(context, original.Bytes()); break;
    case 0x822906e0u: __imp__sub_822906E0(context, original.Bytes()); break;
#endif
    }
    active = nullptr;
    const auto object = indirect ? item.indirect_target : member_address;
#ifdef LO_CALLER_FRAME_VTABLE_PPC9_ONLY
    if (observed_calls != 1)
        throw std::runtime_error("caller frame vtable lower call count");
#endif
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
#ifdef LO_CALLER_FRAME_VTABLE_PPC9_ONLY
    if (context.r3.u64 != expected_object || context.r1.u64 != expected_sp ||
        context.lr != expected_return)
        throw std::runtime_error("caller frame vtable lower call arguments");
    ++observed_calls;
#endif
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
