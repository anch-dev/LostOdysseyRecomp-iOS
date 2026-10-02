#pragma once

#include <cstdint>

namespace lo::semantic::gpu::detail
{

// Store policy separates bounded comparison memory from PPC's volatile stores.
// Both paths use the same recovered alignment, width and ordering decisions.
template<class Memory>
void FillMemory(Memory& memory, std::uint32_t cursor, std::uint32_t value,
    std::uint32_t bytes)
{
    const auto byte = static_cast<std::uint8_t>(value);
    while (bytes != 0 && (cursor & 3) != 0)
    {
        memory.WriteU8(cursor++, byte);
        --bytes;
    }

    const std::uint32_t word = std::uint32_t{byte} * 0x01010101u;
    while (bytes >= 16)
    {
        memory.WriteU32(cursor, word);
        memory.WriteU32(cursor + 4, word);
        memory.WriteU32(cursor + 8, word);
        memory.WriteU32(cursor + 12, word);
        cursor += 16;
        bytes -= 16;
    }
    while (bytes >= 4)
    {
        memory.WriteU32(cursor, word);
        cursor += 4;
        bytes -= 4;
    }
    while (bytes != 0)
    {
        memory.WriteU8(cursor++, byte);
        --bytes;
    }
}

} // namespace lo::semantic::gpu::detail
