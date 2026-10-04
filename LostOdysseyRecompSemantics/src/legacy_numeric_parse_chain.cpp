#include "lo_semantics/legacy_numeric_parse_chain.h"

#include <stdexcept>

namespace lo::semantic::gpu::legacy_numeric_parse_chain
{
namespace
{
namespace parser = legacy_float_text_parser;
namespace routes = legacy_float_parse_routes;

class ParserUnresolved final : public parser::PpcBoundaryServices
{
public:
    ParserUnresolved(routes::PpcBoundaryServices& services,
        routes::Registers& state) : services_(services), state_(state) {}

    void DigitsToExtended82297F38(GuestMemory&,
        parser::Registers&) override
    { throw std::runtime_error("digit call escaped actual-body chain"); }

    void InvalidArgument82B7FD78(GuestMemory& memory,
        parser::Registers& registers) override
    {
        state_.integer = registers;
        services_.InvalidArgument82B7FD78(memory, state_);
        registers = state_.integer;
    }

    void InvalidParameter82B7FEC0(GuestMemory& memory,
        parser::Registers& registers) override
    {
        state_.integer = registers;
        services_.InvalidParameter82B7FEC0(memory, state_);
        registers = state_.integer;
    }

private:
    routes::PpcBoundaryServices& services_;
    routes::Registers& state_;
};
} // namespace

Services::Services(routes::PpcBoundaryServices& unresolved,
    std::array<DigitCondition, 8> initial_cr)
    : unresolved_(unresolved), cr_(initial_cr) {}

void Services::Parse822975B0(GuestMemory& memory, routes::Registers& state)
{
    ParserUnresolved parser_unresolved(unresolved_, state);
    legacy_float_text_parser_digit_chain::Services digits(parser_unresolved,
        cr_);
    auto registers = state.integer;
    if (!parser::Apply(0x822975b0u, memory, digits, registers))
        throw std::runtime_error("recovered 822975B0 entry missing");
    state.integer = registers;
    digit_calls_ += digits.DigitCalls();
    ++parse_calls_;
}

void Services::Classify822981C8(GuestMemory&, routes::Registers&)
{ throw std::runtime_error("822981C8 callback must not run"); }

void Services::InvalidArgument82B7FD78(GuestMemory& memory,
    routes::Registers& state)
{ unresolved_.InvalidArgument82B7FD78(memory, state); }

void Services::InvalidParameter82B7FEC0(GuestMemory& memory,
    routes::Registers& state)
{ unresolved_.InvalidParameter82B7FEC0(memory, state); }

void Services::SetHostFpControl(std::uint32_t control)
{ unresolved_.SetHostFpControl(control); }
} // namespace lo::semantic::gpu::legacy_numeric_parse_chain
