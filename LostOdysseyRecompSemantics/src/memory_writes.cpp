#include "lo_semantics/memory_writes.h"

namespace lo::semantic::memory_writes
{
bool Apply(std::uint32_t address, Registers& registers, gpu::GuestMemory& memory)
{
    return ApplyWith(address, registers, memory);
}
} // namespace lo::semantic::memory_writes
