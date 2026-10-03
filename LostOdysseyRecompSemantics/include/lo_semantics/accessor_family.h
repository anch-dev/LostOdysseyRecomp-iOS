#pragma once

#include "lo_semantics/guest_memory.h"

#include <cstdint>

namespace lo::semantic::gpu
{

enum class IntegerWidth : std::uint8_t
{
    Byte = 1,
    Halfword = 2,
    Word = 4,
};

// Read a fixed-offset unsigned field and zero-extend it to the return register.
// Address calculation uses the low 32 bits of the base and wraps at 32 bits.
[[nodiscard]] std::uint64_t ReadField(GuestMemory& memory, GuestAddress base,
                                      std::int32_t displacement, IntegerWidth width);

// Write the low bits of value to a fixed-offset field. Caller registers do not change.
void WriteField(GuestMemory& memory, GuestAddress base, std::int32_t displacement,
                IntegerWidth width, std::uint64_t value);

} // namespace lo::semantic::gpu
