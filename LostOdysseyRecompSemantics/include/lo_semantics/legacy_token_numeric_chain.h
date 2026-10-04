#pragma once

#include "lo_semantics/legacy_numeric_parse_chain.h"
#include "lo_semantics/legacy_token_float_cursor.h"

#include <array>

namespace lo::semantic::gpu::legacy_token_numeric_chain
{
// Replaces the token cursor's 82B7D270 conversion callback with the accepted
// numeric route/parser/digit/binary chain. The cursor's other dependencies
// remain the existing accepted metadata-search and UTF-16 length semantics.
class Numbers final : public legacy_token_float_cursor::NumberServices
{
public:
    using DigitCondition = legacy_float_digit_accumulator::Condition;

    Numbers(GuestMemory& memory,
        legacy_token_float_cursor::Registers& state,
        legacy_float_parse_routes::PpcBoundaryServices& unresolved,
        std::array<DigitCondition, 8> initial_cr);

    double Convert(std::uint64_t text, std::uint64_t mode) override;
    void SetHostFpControl(std::uint32_t control) override;

    [[nodiscard]] unsigned Conversions() const { return conversions_; }
    [[nodiscard]] unsigned ParseCalls() const { return numeric_.ParseCalls(); }
    [[nodiscard]] unsigned DigitCalls() const { return numeric_.DigitCalls(); }

private:
    GuestMemory& memory_;
    legacy_token_float_cursor::Registers& state_;
    legacy_float_parse_routes::PpcBoundaryServices& unresolved_;
    legacy_numeric_parse_chain::Services numeric_;
    unsigned conversions_ = 0;
};
} // namespace lo::semantic::gpu::legacy_token_numeric_chain
