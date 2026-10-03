#include "lo_semantics/crt_status_error.h"

namespace lo::semantic::gpu::crt_status_error
{
namespace
{
GuestAddress Address(std::uint64_t value)
{
    return static_cast<GuestAddress>(value);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (address != 0x827ca628u) return false;

    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    memory.WriteU32(Address(state.sp - 96u), Address(state.sp));
    state.sp -= 96u;
    state.lr = 0x827ca638u;
    native.NtStatusToDosError(memory, state);

    state.r11 = memory.ReadU32(Address(state.r13 + 336u));
    const bool blocked = Address(state.r11) != 0;
    state.cr6 = {0, static_cast<std::uint8_t>(blocked),
        static_cast<std::uint8_t>(!blocked), state.xer_so};
    if (!blocked)
    {
        state.r11 = memory.ReadU32(Address(state.r13 + 256u));
        memory.WriteU32(Address(state.r11 + 352u), Address(state.r3));
    }

    // The original adds 96 to live full SP; it does not reload its backchain.
    state.sp += 96u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    return true;
}
} // namespace lo::semantic::gpu::crt_status_error
