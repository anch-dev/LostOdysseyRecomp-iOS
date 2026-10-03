#include "lo_semantics/field_bits.h"

namespace lo::semantic::field_bits
{

bool Apply(std::uint32_t address, Registers& registers, gpu::GuestMemory& memory)
{
    return ApplyWith(address, registers, memory);
}

} // namespace lo::semantic::field_bits
