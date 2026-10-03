#include "lo_semantics/crt_float_sign.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>

namespace lo::semantic::gpu::crt_float_sign
{
bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry != 0x82b7efb0u)
        return false;
    state.r11 = 0xffffffff82000000ull;
    constexpr std::uint32_t FlushMask = 0x8040u;
    if ((state.cached_fp_control & FlushMask) != 0)
    {
        state.cached_fp_control &= ~FlushMask;
        native.SetHostFpControl(state.cached_fp_control);
    }
    state.f13_bits = recovery_abi::ReadU64(memory,
        recovery_abi::Address(state.r3));
    state.r3 = 1;
    state.f0_bits = recovery_abi::ReadU64(memory,
        recovery_abi::Address(state.r11 + 4072u));
    const auto value = std::bit_cast<double>(state.f13_bits);
    const auto reference = std::bit_cast<double>(state.f0_bits);
    const bool unordered = std::isnan(value) || std::isnan(reference);
    state.cr6 = {std::uint8_t(!unordered && value < reference),
        std::uint8_t(!unordered && value > reference),
        std::uint8_t(!unordered && value == reference), std::uint8_t(unordered)};
    if (state.cr6.less != 0)
        state.r3 = 0;
    return true;
}
} // namespace lo::semantic::gpu::crt_float_sign
