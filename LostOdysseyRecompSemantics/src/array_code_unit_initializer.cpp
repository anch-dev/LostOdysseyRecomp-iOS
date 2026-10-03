#include "lo_semantics/array_code_unit_initializer.h"

#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::array_code_unit_initializer
{
bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, Registers& state)
{
    if (address != 0x822954d8u) return false;

    using recovery_abi::Address;
    using recovery_abi::ReadU64;
    using recovery_abi::WriteU64;

    const auto incoming_sp = Address(state.sp);
    state.r12 = state.lr;
    memory.WriteU32(incoming_sp - 8u, Address(state.r12));
    WriteU64(memory, incoming_sp - 16u, state.r31);
    memory.WriteU32(Address(state.sp - 96u), incoming_sp);
    state.sp -= 96u;

    state.r31 = state.r3;
    state.r11 = state.r4;
    state.r10 = 0;
    state.r5 = 8;
    state.r4 = 2;
    const auto array = Address(state.r31);
    memory.WriteU32(array + 4u, Address(state.r11));
    memory.WriteU32(array, Address(state.r10));
    memory.WriteU32(array + 8u, Address(state.r11));

    state.lr = 0x8229550cu;
    ResizeArray(memory, services, Address(state.r3),
        Address(state.r4), Address(state.r5));

    state.r3 = state.r31;
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    state.r31 = ReadU64(memory, Address(state.sp - 16u));
    return true;
}
} // namespace lo::semantic::gpu::array_code_unit_initializer
