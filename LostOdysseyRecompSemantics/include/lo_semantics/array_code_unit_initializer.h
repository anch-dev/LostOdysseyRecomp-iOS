#pragma once

#include "lo_semantics/allocation_array.h"
#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu::array_code_unit_initializer
{
struct Registers
{
    std::uint64_t sp = 0, lr = 0;
    std::uint64_t r3 = 0, r4 = 0, r5 = 0;
    std::uint64_t r10 = 0, r11 = 0, r12 = 0, r31 = 0;
};

// 822954D8 initializes an array header for two-byte code units, asks the
// accepted 8229F678 model to resize storage, then returns the full original
// array pointer. Unknown addresses leave the selected context and RAM intact.
[[nodiscard]] bool Apply(GuestAddress address, GuestMemory& memory,
    ArrayResizeServices& services, Registers& registers);
} // namespace lo::semantic::gpu::array_code_unit_initializer
