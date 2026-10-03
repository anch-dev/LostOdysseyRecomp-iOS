#include "lo_semantics/single_write_fields.h"

namespace lo::semantic::single_write_fields
{
bool Apply(std::uint32_t address, Registers& registers, gpu::GuestMemory& memory)
{
    return ApplyWith(address, registers, memory);
}
} // namespace lo::semantic::single_write_fields
