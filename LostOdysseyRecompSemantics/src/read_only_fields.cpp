#include "lo_semantics/read_only_fields.h"

namespace lo::semantic::read_only_fields
{
bool Apply(std::uint32_t address, Registers& registers, gpu::GuestMemory& memory)
{
    return ApplyWith(address, registers, memory);
}
} // namespace lo::semantic::read_only_fields
