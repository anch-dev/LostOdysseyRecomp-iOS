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

// Share the field operation with backends that retain the guest access width.
// Memory supplies ReadU8/U16/U32 and WriteU8/U16/U32; it owns access policy.
template<class Memory>
[[nodiscard]] std::uint64_t ReadFieldWith(Memory& memory, GuestAddress base,
                                        std::int32_t displacement, IntegerWidth width)
{
    const GuestAddress address = base + static_cast<GuestAddress>(displacement);
    switch (width)
    {
    case IntegerWidth::Byte: return memory.ReadU8(address);
    case IntegerWidth::Halfword: return memory.ReadU16(address);
    case IntegerWidth::Word: return memory.ReadU32(address);
    }
    throw std::invalid_argument("unsupported field width");
}

template<class Memory>
void WriteFieldWith(Memory& memory, GuestAddress base, std::int32_t displacement,
                    IntegerWidth width, std::uint64_t value)
{
    const GuestAddress address = base + static_cast<GuestAddress>(displacement);
    switch (width)
    {
    case IntegerWidth::Byte: memory.WriteU8(address, static_cast<std::uint8_t>(value)); return;
    case IntegerWidth::Halfword: memory.WriteU16(address, static_cast<std::uint16_t>(value)); return;
    case IntegerWidth::Word: memory.WriteU32(address, static_cast<std::uint32_t>(value)); return;
    }
    throw std::invalid_argument("unsupported field width");
}

// Read a fixed-offset unsigned field and zero-extend it to the return register.
// Address calculation uses the low 32 bits of the base and wraps at 32 bits.
[[nodiscard]] std::uint64_t ReadField(GuestMemory& memory, GuestAddress base,
                                      std::int32_t displacement, IntegerWidth width);

// Write the low bits of value to a fixed-offset field. Caller registers do not change.
void WriteField(GuestMemory& memory, GuestAddress base, std::int32_t displacement,
                IntegerWidth width, std::uint64_t value);

} // namespace lo::semantic::gpu
