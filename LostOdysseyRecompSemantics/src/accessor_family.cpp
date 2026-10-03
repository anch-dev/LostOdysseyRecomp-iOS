#include "lo_semantics/accessor_family.h"

#include <stdexcept>

namespace lo::semantic::gpu
{

std::uint64_t ReadField(GuestMemory& memory, GuestAddress base,
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

void WriteField(GuestMemory& memory, GuestAddress base, std::int32_t displacement,
                IntegerWidth width, std::uint64_t value)
{
    const GuestAddress address = base + static_cast<GuestAddress>(displacement);
    switch (width)
    {
    case IntegerWidth::Byte:
        memory.WriteU8(address, static_cast<std::uint8_t>(value));
        return;
    case IntegerWidth::Halfword:
        memory.WriteU16(address, static_cast<std::uint16_t>(value));
        return;
    case IntegerWidth::Word:
        memory.WriteU32(address, static_cast<std::uint32_t>(value));
        return;
    }
    throw std::invalid_argument("unsupported field width");
}

} // namespace lo::semantic::gpu
