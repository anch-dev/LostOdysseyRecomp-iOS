#include "lo_semantics/record_buffer_extent.h"
#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::record_buffer_extent
{
bool Apply(GuestAddress entry, GuestMemory& memory, Registers& state)
{
    if (entry != 0x82373190u) return false;
    state.r[11] = memory.ReadU32(recovery_abi::Address(state.r[3] + 252u));
    state.r[10] = memory.ReadU32(recovery_abi::Address(state.r[3] + 508u));
    state.r[11] = recovery_abi::WordRotateMask(state.r[11], 2, 0xfffffffcu);
    const auto flag = static_cast<std::uint32_t>(state.r[10]);
    state.cr6 = {0, std::uint8_t(flag != 0), std::uint8_t(flag == 0),
        state.xer_so};
    if (!state.cr6.eq) state.r[11] = 12u;
    state.r[3] = state.r[11] + 576u;
    return true;
}
}
