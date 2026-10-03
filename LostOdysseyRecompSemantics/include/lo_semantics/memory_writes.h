#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::memory_writes
{

// The generated PPC uses full 64-bit temporaries even when guest addresses and
// stored words are 32-bit. Keep the touched registers visible to the caller.
struct Registers
{
    std::uint64_t r3 = 0, r4 = 0, r5 = 0, r6 = 0, r7 = 0;
    std::uint64_t r8 = 0, r9 = 0, r10 = 0, r11 = 0;
    std::uint64_t r1 = 0;
};

// Returns false, without changing registers or memory, for an unknown address.
bool Apply(std::uint32_t address, Registers& registers, gpu::GuestMemory& memory);

} // namespace lo::semantic::memory_writes
