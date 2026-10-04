#include "lo_semantics/legacy_float_public_numeric_parse_chain.h"

#include <stdexcept>

namespace lo::semantic::gpu::legacy_float_public_numeric_parse_chain
{
namespace
{
namespace public_parse = legacy_float_public_parse;
namespace parser = legacy_float_text_parser;
namespace digit_chain = legacy_float_text_parser_digit_chain;

legacy_float_digit_accumulator::Condition ToDigitCondition(
    const crt_stream_operations::Condition& cr)
{ return {cr.lt, cr.gt, cr.eq, cr.so}; }

class ParserServices final : public parser::PpcBoundaryServices
{
public:
    ParserServices(public_parse::NativeServices& unresolved,
        public_parse::Registers& outer)
        : unresolved_(unresolved), outer_(outer) {}

    void DigitsToExtended82297F38(GuestMemory&,
        parser::Registers&) override
    { throw std::logic_error("82297F38 must use its recovered lower"); }

    void InvalidArgument82B7FD78(GuestMemory& memory,
        parser::Registers& state) override
    {
        outer_.route.integer = state;
        unresolved_.InvalidArgument82B7FD78(memory, outer_);
        state = outer_.route.integer;
    }

    void InvalidParameter82B7FEC0(GuestMemory& memory,
        parser::Registers& state) override
    {
        outer_.route.integer = state;
        unresolved_.InvalidParameter82B7FEC0(memory, outer_);
        state = outer_.route.integer;
    }

private:
    public_parse::NativeServices& unresolved_;
    public_parse::Registers& outer_;
};
} // namespace

Services::Services(public_parse::NativeServices& unresolved,
    Conditions initial_cr)
    : unresolved_(unresolved), cr_(initial_cr) {}

void Services::Parse822975B0(GuestMemory& memory,
    public_parse::Registers& state)
{
    ParserServices unresolved(unresolved_, state);
    digit_chain::Services digits(unresolved, cr_);
    auto parser_state = state.route.integer;
    if (!parser::Apply(0x822975b0u, memory, digits, parser_state))
        throw std::logic_error("recovered 822975B0 entry missing");
    state.route.integer = parser_state;
    if (digits.DigitCalls())
        cr_ = digits.LastDigitReturn().cr;
    cr_[0] = ToDigitCondition(parser_state.cr0);
    cr_[6] = ToDigitCondition(parser_state.cr6);
    ++parser_calls_;
    digit_calls_ += digits.DigitCalls();
}

void Services::InvalidArgument82B7FD78(GuestMemory& memory,
    public_parse::Registers& state)
{ unresolved_.InvalidArgument82B7FD78(memory, state); }

void Services::InvalidParameter82B7FEC0(GuestMemory& memory,
    public_parse::Registers& state)
{ unresolved_.InvalidParameter82B7FEC0(memory, state); }

void Services::SetHostFpControl(std::uint32_t control)
{ unresolved_.SetHostFpControl(control); }
} // namespace lo::semantic::gpu::legacy_float_public_numeric_parse_chain
