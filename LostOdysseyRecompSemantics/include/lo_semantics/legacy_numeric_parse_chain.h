#pragma once

#include "lo_semantics/legacy_float_parse_routes.h"
#include "lo_semantics/legacy_float_text_parser_digit_chain.h"

#include <array>

namespace lo::semantic::gpu::legacy_numeric_parse_chain
{
// Connects the accepted 822974F8 route to the accepted 822975B0 parser and
// 82297F38 digit lower. The route and parser expose CR0/CR6, while the digit
// lower carries all eight CR fields internally. This adapter does not claim
// end-to-end CR1/CR7 fidelity through the outer route ABI.
class Services final : public legacy_float_parse_routes::PpcBoundaryServices
{
public:
    using DigitCondition = legacy_float_digit_accumulator::Condition;

    Services(legacy_float_parse_routes::PpcBoundaryServices& unresolved,
        std::array<DigitCondition, 8> initial_cr);

    void Parse822975B0(GuestMemory& memory,
        legacy_float_parse_routes::Registers& state) override;
    void Classify822981C8(GuestMemory& memory,
        legacy_float_parse_routes::Registers& state) override;
    void InvalidArgument82B7FD78(GuestMemory& memory,
        legacy_float_parse_routes::Registers& state) override;
    void InvalidParameter82B7FEC0(GuestMemory& memory,
        legacy_float_parse_routes::Registers& state) override;
    void SetHostFpControl(std::uint32_t control) override;

    [[nodiscard]] unsigned ParseCalls() const { return parse_calls_; }
    [[nodiscard]] unsigned DigitCalls() const { return digit_calls_; }

private:
    legacy_float_parse_routes::PpcBoundaryServices& unresolved_;
    std::array<DigitCondition, 8> cr_;
    unsigned parse_calls_ = 0, digit_calls_ = 0;
};
} // namespace lo::semantic::gpu::legacy_numeric_parse_chain
