#pragma once

#include "lo_semantics/legacy_float_digit_accumulator.h"
#include "lo_semantics/legacy_float_text_parser.h"

#include <array>

namespace lo::semantic::gpu::legacy_float_text_parser_digit_chain
{
// Supplies the recovered 82297F38 PPC body at the parser's digit-conversion
// call. CR fields other than CR0/CR6 are carried across that call explicitly;
// the parser's public selected-state interface does not expose them.
class Services final : public legacy_float_text_parser::PpcBoundaryServices
{
public:
    using DigitRegisters = legacy_float_digit_accumulator::Registers;
    using DigitCondition = legacy_float_digit_accumulator::Condition;

    Services(legacy_float_text_parser::PpcBoundaryServices& unresolved,
        std::array<DigitCondition, 8> initial_cr);

    void DigitsToExtended82297F38(GuestMemory& memory,
        legacy_float_text_parser::Registers& state) override;
    void InvalidArgument82B7FD78(GuestMemory& memory,
        legacy_float_text_parser::Registers& state) override;
    void InvalidParameter82B7FEC0(GuestMemory& memory,
        legacy_float_text_parser::Registers& state) override;

    [[nodiscard]] unsigned DigitCalls() const { return digit_calls_; }
    [[nodiscard]] const DigitRegisters& LastDigitReturn() const
    { return last_digit_return_; }

private:
    legacy_float_text_parser::PpcBoundaryServices& unresolved_;
    std::array<DigitCondition, 8> cr_;
    DigitRegisters last_digit_return_{};
    unsigned digit_calls_ = 0;
};
} // namespace lo::semantic::gpu::legacy_float_text_parser_digit_chain
