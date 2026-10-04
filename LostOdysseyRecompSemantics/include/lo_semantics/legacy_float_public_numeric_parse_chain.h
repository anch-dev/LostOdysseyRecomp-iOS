#pragma once

#include "lo_semantics/legacy_float_public_parse.h"
#include "lo_semantics/legacy_float_text_parser_digit_chain.h"

#include <array>

namespace lo::semantic::gpu::legacy_float_public_numeric_parse_chain
{
// Replaces the public parser boundary with accepted 822975B0, 82297F38 and
// their memory-copy lower. The unresolved service handles only exceptional
// native calls and host FP control. Non-CR0/CR6 fields cross the digit call
// through the initial/final condition array because the public family exposes
// only those two condition fields.
class Services final : public legacy_float_public_parse::NativeServices
{
public:
    using DigitCondition = legacy_float_digit_accumulator::Condition;
    using Conditions = std::array<DigitCondition, 8>;

    Services(legacy_float_public_parse::NativeServices& unresolved,
        Conditions initial_cr);

    void Parse822975B0(GuestMemory& memory,
        legacy_float_public_parse::Registers& state) override;
    void InvalidArgument82B7FD78(GuestMemory& memory,
        legacy_float_public_parse::Registers& state) override;
    void InvalidParameter82B7FEC0(GuestMemory& memory,
        legacy_float_public_parse::Registers& state) override;
    void SetHostFpControl(std::uint32_t control) override;

    [[nodiscard]] unsigned ParserCalls() const { return parser_calls_; }
    [[nodiscard]] unsigned DigitCalls() const { return digit_calls_; }
    [[nodiscard]] const Conditions& FinalConditions() const { return cr_; }

private:
    legacy_float_public_parse::NativeServices& unresolved_;
    Conditions cr_;
    unsigned parser_calls_ = 0;
    unsigned digit_calls_ = 0;
};
} // namespace lo::semantic::gpu::legacy_float_public_numeric_parse_chain
