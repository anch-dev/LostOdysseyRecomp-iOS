#include "lo_semantics/caller_frame_vtable_init.h"
#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::caller_frame_vtable_init
{
namespace
{
using recovery_abi::Address;

void InitializeDirectVTable(GuestMemory& memory, Registers& state)
{
    recovery_abi::WriteU64(memory, Address(state.sp - 8u), state.r31);
    state.r31 = state.sp - 16u;
    memory.WriteU32(Address(state.r31), Address(state.sp));
    state.sp = state.r31;
    memory.WriteU32(Address(state.r31 + 36u), Address(state.r3));
    state.r11 = 0xffffffff8208018cull;
    memory.WriteU32(Address(state.r3), Address(state.r11));
    state.sp = state.r31 + 16u;
    state.r31 = recovery_abi::ReadU64(memory, Address(state.sp - 8u));
}

void InitializeCallerFrameMember(GuestMemory& memory, Registers& state,
    std::uint32_t frame_size, std::uint32_t member_offset, GuestAddress return_address)
{
    state.r31 = state.r12 - frame_size;
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    state.r3 = state.r31 + member_offset;
    state.lr = return_address;
    InitializeDirectVTable(memory, state);
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
}
}

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    switch (entry)
    {
    case 0x828138f8u: InitializeDirectVTable(memory, state); return true;
    case 0x82373104u: InitializeCallerFrameMember(memory, state, 160u, 104u, 0x8237311cu); return true;
    case 0x8237dafcu: InitializeCallerFrameMember(memory, state, 144u, 80u, 0x8237db14u); return true;
    default: return false;
    }
}
}
