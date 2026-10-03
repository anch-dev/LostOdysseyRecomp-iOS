#include "lo_semantics/caller_frame_vtable_restore.h"
#include "lo_semantics/pointer_fields.h"
#include "lo_semantics/recovery_abi.h"

#include <array>

namespace lo::semantic::gpu::caller_frame_vtable_restore
{
namespace
{
using recovery_abi::Address;

void RestoreFrameMemberVTable(GuestMemory& memory, Registers& state,
    std::uint32_t frame_size, std::uint32_t member_offset,
    bool indirect, GuestAddress return_address)
{
    state.r31 = state.r12 - frame_size;
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    const auto member = state.r31 + member_offset;
    state.r3 = indirect ? memory.ReadU32(Address(member)) : member;
    state.lr = return_address;
    constexpr std::array<ConstantFieldAssignment, 2> assignments{{
        {PointerFieldRegister::R11, 0xffffffff82180000ull},
        {PointerFieldRegister::R11, 0xffffffff8218018cull}}};
    constexpr std::array<ConstantFieldWrite, 1> writes{{
        {0, PointerFieldWidth::Word, 0x8218018cu}}};
    PointerFieldRegisters call{};
    call.r3 = state.r3;
    call.r11 = state.r11;
    InitializeConstantFields(memory, call, PointerFieldRegister::R3, assignments, writes);
    state.r3 = call.r3;
    state.r11 = call.r11;
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    switch (entry)
    {
    case 0x82290668u: RestoreFrameMemberVTable(memory, state, 512u, 88u, true, 0x82290680u); return true;
    case 0x82290690u: RestoreFrameMemberVTable(memory, state, 512u, 88u, false, 0x822906a8u); return true;
    case 0x822906e0u: RestoreFrameMemberVTable(memory, state, 512u, 88u, false, 0x822906f8u); return true;
    case 0x823730b4u: RestoreFrameMemberVTable(memory, state, 160u, 80u, true, 0x823730ccu); return true;
    case 0x823730dcu: RestoreFrameMemberVTable(memory, state, 160u, 104u, false, 0x823730f4u); return true;
    case 0x8237312cu: RestoreFrameMemberVTable(memory, state, 160u, 104u, false, 0x82373144u); return true;
    case 0x8237daacu: RestoreFrameMemberVTable(memory, state, 144u, 80u, true, 0x8237dac4u); return true;
    case 0x8237dad4u: RestoreFrameMemberVTable(memory, state, 144u, 80u, false, 0x8237daecu); return true;
    case 0x8237db24u: RestoreFrameMemberVTable(memory, state, 144u, 80u, false, 0x8237db3cu); return true;
    default: return false;
    }
}
}
