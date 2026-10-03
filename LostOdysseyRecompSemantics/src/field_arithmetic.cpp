#include "lo_semantics/field_arithmetic.h"

namespace lo::semantic::field_arithmetic
{

bool Apply(std::uint32_t address, Registers& registers, gpu::GuestMemory& memory)
{
    return ApplyWith(address, registers, memory);
}

} // namespace lo::semantic::field_arithmetic
