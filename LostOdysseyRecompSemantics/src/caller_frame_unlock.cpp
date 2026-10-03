#include "lo_semantics/caller_frame_unlock.h"
#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::caller_frame_unlock
{
namespace
{
using recovery_abi::Address;

void ReleaseLockRecord(GuestMemory& memory, NativeServices& native, Registers& state)
{
    state.r11 = memory.ReadU32(Address(state.r3));
    state.r3 = state.r11 + 4u;
    native.LeaveCriticalSection(memory, state);
}
void ReleaseFrameRecord(GuestMemory& memory, NativeServices& native,
    Registers& state, std::uint32_t caller_frame_size,
    std::uint32_t field_offset, GuestAddress return_address)
{
    state.r31 = state.r12 - caller_frame_size;
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    state.r3 = state.r31 + field_offset;
    state.lr = return_address;
    ReleaseLockRecord(memory, native, state);
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
}
}

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    switch (entry)
    {
    case 0x82290878u: ReleaseLockRecord(memory, native, state); return true;
    case 0x82290708u: ReleaseFrameRecord(memory, native, state, 512, 88, 0x82290720u); return true;
    case 0x8229587cu: ReleaseFrameRecord(memory, native, state, 176, 80, 0x82295894u); return true;
    case 0x822958a4u: ReleaseFrameRecord(memory, native, state, 176, 80, 0x822958bcu); return true;
    case 0x822958ccu: ReleaseFrameRecord(memory, native, state, 176, 80, 0x822958e4u); return true;
    case 0x8229a8dcu: ReleaseFrameRecord(memory, native, state, 128, 80, 0x8229a8f4u); return true;
    case 0x8229a904u: ReleaseFrameRecord(memory, native, state, 128, 80, 0x8229a91cu); return true;
    case 0x822c3cfcu: ReleaseFrameRecord(memory, native, state, 128, 80, 0x822c3d14u); return true;
    default: return false;
    }
}
}
