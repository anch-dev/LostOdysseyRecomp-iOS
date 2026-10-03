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
    bool indirect, GuestAddress return_address)
{
    state.r31 = state.r12 - 512u;
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    state.r3 = indirect ? memory.ReadU32(Address(state.r31 + 88u)) : state.r31 + 88u;
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
    case 0x82290668u: RestoreFrameMemberVTable(memory, state, true, 0x82290680u); return true;
    case 0x82290690u: RestoreFrameMemberVTable(memory, state, false, 0x822906a8u); return true;
    case 0x822906e0u: RestoreFrameMemberVTable(memory, state, false, 0x822906f8u); return true;
    default: return false;
    }
}
}
