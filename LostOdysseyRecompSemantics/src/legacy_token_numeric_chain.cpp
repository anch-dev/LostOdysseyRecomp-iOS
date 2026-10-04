#include "lo_semantics/legacy_token_numeric_chain.h"

#include <bit>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_token_numeric_chain
{
Numbers::Numbers(GuestMemory& memory,
    legacy_token_float_cursor::Registers& state,
    legacy_float_parse_routes::PpcBoundaryServices& unresolved,
    std::array<DigitCondition, 8> initial_cr)
    : memory_(memory), state_(state), unresolved_(unresolved),
      numeric_(unresolved, initial_cr) {}

double Numbers::Convert(std::uint64_t text, std::uint64_t mode)
{
    if (state_.integer.r[3] != text || state_.integer.r[4] != mode)
        throw std::runtime_error("token conversion register arguments changed");
    legacy_float_parse_routes::Registers nested{};
    nested.integer = state_.integer;
    nested.f1_bits = state_.f1_bits;
    nested.cached_fp_control = state_.cached_fp_control;
    if (!legacy_float_parse_routes::Apply(0x82b7d270u,
        memory_, numeric_, nested))
        throw std::runtime_error("recovered 82B7D270 entry missing");
    state_.integer = nested.integer;
    state_.f1_bits = nested.f1_bits;
    state_.cached_fp_control = nested.cached_fp_control;
    ++conversions_;
    return std::bit_cast<double>(nested.f1_bits);
}

void Numbers::SetHostFpControl(std::uint32_t control)
{ unresolved_.SetHostFpControl(control); }
} // namespace lo::semantic::gpu::legacy_token_numeric_chain
