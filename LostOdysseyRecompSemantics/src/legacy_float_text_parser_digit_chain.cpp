#include "lo_semantics/legacy_float_text_parser_digit_chain.h"

#include <stdexcept>

namespace lo::semantic::gpu::legacy_float_text_parser_digit_chain
{
namespace
{
using ParserRegisters = legacy_float_text_parser::Registers;
using DigitCondition = legacy_float_digit_accumulator::Condition;

DigitCondition ToDigitCondition(const crt_stream_operations::Condition& cr)
{ return {cr.lt, cr.gt, cr.eq, cr.so}; }

crt_stream_operations::Condition ToParserCondition(const DigitCondition& cr)
{ return {cr.lt, cr.gt, cr.eq, cr.so}; }
} // namespace

Services::Services(legacy_float_text_parser::PpcBoundaryServices& unresolved,
    std::array<DigitCondition, 8> initial_cr)
    : unresolved_(unresolved), cr_(initial_cr) {}

void Services::DigitsToExtended82297F38(GuestMemory& memory,
    ParserRegisters& state)
{
    const auto opaque_r1 = state.r[1];
    DigitRegisters digit{};
    digit.r = state.r;
    digit.r[1] = state.sp;
    digit.lr = state.lr;
    digit.ctr = state.ctr;
    digit.cr = cr_;
    digit.cr[0] = ToDigitCondition(state.cr0);
    digit.cr[6] = ToDigitCondition(state.cr6);
    digit.xer_so = state.xer_so;
    digit.xer_ca = state.xer_ca;
    if (!legacy_float_digit_accumulator::Apply(0x82297f38u, memory, digit))
        throw std::runtime_error("recovered 82297F38 entry missing");
    state.r = digit.r;
    state.r[1] = opaque_r1; // Parser represents r1 in sp, not in r[1].
    state.sp = digit.r[1];
    state.lr = digit.lr;
    state.ctr = digit.ctr;
    state.cr0 = ToParserCondition(digit.cr[0]);
    state.cr6 = ToParserCondition(digit.cr[6]);
    state.xer_so = digit.xer_so;
    state.xer_ca = digit.xer_ca;
    cr_ = digit.cr;
    last_digit_return_ = digit;
    ++digit_calls_;
}

void Services::InvalidArgument82B7FD78(GuestMemory& memory,
    ParserRegisters& state)
{ unresolved_.InvalidArgument82B7FD78(memory, state); }

void Services::InvalidParameter82B7FEC0(GuestMemory& memory,
    ParserRegisters& state)
{ unresolved_.InvalidParameter82B7FEC0(memory, state); }
} // namespace lo::semantic::gpu::legacy_float_text_parser_digit_chain
