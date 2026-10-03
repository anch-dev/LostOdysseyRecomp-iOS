#include "lo_semantics/float_triplet_transfer.h"
#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

namespace lo::semantic::gpu::float_triplet_transfer
{
bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry != 0x822c4d18u && entry != 0x822c4dd0u)
        return false;

    constexpr std::uint32_t FlushMask = 0x8040u;
    if ((state.cached_fp_control & FlushMask) != 0)
    {
        state.cached_fp_control &= ~FlushMask;
        native.SetHostFpControl(state.cached_fp_control);
    }

    const bool to_object = entry == 0x822c4d18u;
    const auto source = recovery_abi::Address(
        to_object ? state.r4 : state.r3 + 1132u);
    const auto destination = recovery_abi::Address(
        to_object ? state.r3 + 80u : state.r4);
    for (std::uint32_t index = 0; index < 3u; ++index)
    {
        const auto value = LoadedSingle::FromWord(
            memory.ReadU32(source + index * 4u));
        state.f0_bits = value.FprBits();
        if (to_object && index == 0u)
            state.r11 = 1u;
        memory.WriteU32(destination + index * 4u, value.StoreWord());
    }
    if (to_object)
        memory.WriteU32(recovery_abi::Address(state.r3 + 92u),
            recovery_abi::Address(state.r11));
    return true;
}
} // namespace lo::semantic::gpu::float_triplet_transfer
