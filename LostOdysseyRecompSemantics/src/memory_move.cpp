#include "lo_semantics/memory_move.h"

#include <bit>

namespace lo::semantic::gpu
{
namespace
{

// GuestMemory has byte and word accessors. Grouping two big-endian words
// models each 64-bit PPC load/store for ordinary bounded guest memory.
std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    const std::uint32_t high = memory.ReadU32(address);
    const std::uint32_t low = memory.ReadU32(address + 4);
    return (std::uint64_t{high} << 32) | low;
}

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4, static_cast<std::uint32_t>(value));
}

void CopyByteForward(GuestMemory& memory, GuestAddress& destination,
    GuestAddress& source)
{
    const std::uint8_t value = memory.ReadU8(source);
    memory.WriteU8(destination, value);
    ++destination;
    ++source;
}

void CopyByteBackward(GuestMemory& memory, GuestAddress& destination_end,
    GuestAddress& source_end)
{
    --destination_end;
    --source_end;
    const std::uint8_t value = memory.ReadU8(source_end);
    memory.WriteU8(destination_end, value);
}

void CopyWordForward(GuestMemory& memory, GuestAddress& destination,
    GuestAddress& source)
{
    const std::uint32_t value = memory.ReadU32(source);
    memory.WriteU32(destination, value);
    destination += 4;
    source += 4;
}

void CopyQwordForward(GuestMemory& memory, GuestAddress& destination,
    GuestAddress& source)
{
    const std::uint64_t value = ReadU64(memory, source);
    WriteU64(memory, destination, value);
    destination += 8;
    source += 8;
}

void CopyAlignedQwordBulk(GuestMemory& memory, GuestAddress& destination,
    GuestAddress& source)
{
    // The PPC loop holds three loaded qwords while it interleaves later
    // loads and stores over one 128-byte cache line.
    std::uint64_t pending[3] = {
        ReadU64(memory, source),
        ReadU64(memory, source + 8),
        ReadU64(memory, source + 16),
    };
    for (std::uint32_t index = 0; index < 13; ++index)
    {
        WriteU64(memory, destination + index * 8, pending[index % 3]);
        pending[index % 3] = ReadU64(memory, source + (index + 3) * 8);
    }
    for (std::uint32_t index = 13; index < 16; ++index)
        WriteU64(memory, destination + index * 8, pending[index % 3]);
    destination += 128;
    source += 128;
}

void CopyAlignedWordBulk(GuestMemory& memory, GuestAddress& destination,
    GuestAddress& source)
{
    for (std::uint32_t group = 0; group < 8; ++group)
    {
        const GuestAddress source_group = source + group * 16;
        const GuestAddress destination_group = destination + group * 16;
        const std::uint32_t first = memory.ReadU32(source_group);
        const std::uint32_t second = memory.ReadU32(source_group + 4);
        const std::uint32_t third = memory.ReadU32(source_group + 8);
        memory.WriteU32(destination_group, first);
        const std::uint32_t fourth = memory.ReadU32(source_group + 12);
        memory.WriteU32(destination_group + 4, second);
        memory.WriteU32(destination_group + 8, third);
        memory.WriteU32(destination_group + 12, fourth);
    }
    destination += 128;
    source += 128;
}

void CopyUnalignedWordForward(GuestMemory& memory, GuestAddress& destination,
    GuestAddress& source)
{
    // The source path at 82B7A4D8 reads bytes high address first, then
    // assembles one big-endian word for a single aligned destination store.
    const std::uint32_t low = memory.ReadU8(source + 3);
    const std::uint32_t third = memory.ReadU8(source + 2);
    const std::uint32_t second = memory.ReadU8(source + 1);
    const std::uint32_t high = memory.ReadU8(source);
    memory.WriteU32(destination, (high << 24) | (second << 16) |
                                  (third << 8) | low);
    destination += 4;
    source += 4;
}

void CopyForward(GuestMemory& memory, GuestAddress destination,
    GuestAddress source, std::uint32_t bytes)
{
    const std::uint32_t destination_offset = destination & 7;
    if (destination_offset != 0)
    {
        const std::uint32_t prefix = 8 - destination_offset;
        if (bytes <= prefix)
        {
            // 82B7A0D8 takes the common short tail before selecting a source
            // alignment path. A four-byte tail first performs a word load.
            if (bytes == 4)
            {
                const std::uint32_t word = memory.ReadU32(source);
                if ((destination & 3) == 0)
                    memory.WriteU32(destination, word);
                else
                {
                    const std::uint8_t first = memory.ReadU8(source);
                    const std::uint8_t second = memory.ReadU8(source + 1);
                    const std::uint8_t third = memory.ReadU8(source + 2);
                    memory.WriteU8(destination, first);
                    memory.WriteU8(destination + 1, second);
                    memory.WriteU8(destination + 2, third);
                    memory.WriteU8(destination + 3, static_cast<std::uint8_t>(word));
                }
            }
            else
                for (std::uint32_t index = 0; index < bytes; ++index)
                    CopyByteForward(memory, destination, source);
            return;
        }
        else
        {
            if (prefix == 4)
                CopyWordForward(memory, destination, source);
            else
                for (std::uint32_t index = 0; index < prefix; ++index)
                    CopyByteForward(memory, destination, source);
            bytes -= prefix;
        }
    }

    const std::uint32_t source_offset = source & 7;
    if (source_offset == 0)
    {
        if (bytes >= 128)
        {
            const std::uint32_t destination_line_offset = destination & 127;
            if (destination_line_offset != 0)
            {
                const std::uint32_t prefix = 128 - destination_line_offset;
                for (std::uint32_t index = 0; index < prefix / 8; ++index)
                    CopyQwordForward(memory, destination, source);
                bytes -= prefix;
            }
            const std::uint32_t lines = bytes / 128;
            for (std::uint32_t index = 0; index < lines; ++index)
                CopyAlignedQwordBulk(memory, destination, source);
            bytes &= 127;
        }

        const std::uint32_t qwords = bytes / 8;
        for (std::uint32_t index = 0; index < qwords; ++index)
            CopyQwordForward(memory, destination, source);
        bytes &= 7;
        if (bytes == 4)
        {
            const std::uint32_t word = memory.ReadU32(source);
            if ((destination & 3) == 0)
                memory.WriteU32(destination, word);
            else
            {
                const std::uint8_t first = memory.ReadU8(source);
                const std::uint8_t second = memory.ReadU8(source + 1);
                const std::uint8_t third = memory.ReadU8(source + 2);
                memory.WriteU8(destination, first);
                memory.WriteU8(destination + 1, second);
                memory.WriteU8(destination + 2, third);
                memory.WriteU8(destination + 3, static_cast<std::uint8_t>(word));
            }
        }
        else
            for (std::uint32_t index = 0; index < bytes; ++index)
                CopyByteForward(memory, destination, source);
    }
    else if (source_offset == 4)
    {
        if (bytes >= 128)
        {
            const std::uint32_t destination_line_offset = destination & 127;
            if (destination_line_offset != 0)
            {
                const std::uint32_t prefix = 128 - destination_line_offset;
                for (std::uint32_t index = 0; index < prefix / 4; ++index)
                    CopyWordForward(memory, destination, source);
                bytes -= prefix;
            }
            const std::uint32_t lines = bytes / 128;
            for (std::uint32_t index = 0; index < lines; ++index)
                CopyAlignedWordBulk(memory, destination, source);
            bytes &= 127;
        }
        const std::uint32_t words = bytes / 4;
        for (std::uint32_t index = 0; index < words; ++index)
            CopyWordForward(memory, destination, source);
        bytes &= 3;
        for (std::uint32_t index = 0; index < bytes; ++index)
            CopyByteForward(memory, destination, source);
    }
    else
    {
        if (bytes >= 128)
        {
            const std::uint32_t destination_line_offset = destination & 127;
            if (destination_line_offset != 0)
            {
                const std::uint32_t prefix = 128 - destination_line_offset;
                for (std::uint32_t index = 0; index < prefix; ++index)
                    CopyByteForward(memory, destination, source);
                bytes -= prefix;
            }
            const std::uint32_t lines = bytes / 128;
            for (std::uint32_t index = 0; index < lines; ++index)
                for (std::uint32_t word = 0; word < 32; ++word)
                    CopyUnalignedWordForward(memory, destination, source);
            bytes &= 127;
        }
        for (std::uint32_t index = 0; index < bytes; ++index)
            CopyByteForward(memory, destination, source);
    }
}

void CopyBackward(GuestMemory& memory, GuestAddress destination,
    GuestAddress source, std::uint32_t bytes)
{
    GuestAddress destination_end = destination + bytes;
    GuestAddress source_end = source + bytes;
    while (bytes != 0 && (destination_end & 3) != 0)
    {
        CopyByteBackward(memory, destination_end, source_end);
        --bytes;
    }

    const std::uint32_t words = bytes / 4;
    for (std::uint32_t index = 0; index < words; ++index)
    {
        if ((source_end & 3) == 0)
        {
            const std::uint32_t value = memory.ReadU32(source_end - 4);
            memory.WriteU32(destination_end - 4, value);
        }
        else
        {
            const std::uint32_t low = memory.ReadU8(source_end - 1);
            const std::uint32_t third = memory.ReadU8(source_end - 2);
            const std::uint32_t second = memory.ReadU8(source_end - 3);
            const std::uint32_t high = memory.ReadU8(source_end - 4);
            memory.WriteU32(destination_end - 4,
                            (high << 24) | (second << 16) | (third << 8) | low);
        }
        destination_end -= 4;
        source_end -= 4;
    }

    bytes &= 3;
    for (std::uint32_t index = 0; index < bytes; ++index)
        CopyByteBackward(memory, destination_end, source_end);
}

} // namespace

std::uint64_t CopyGuestMemory(GuestMemory& memory,
    std::uint64_t destination_register, GuestAddress source,
    std::uint64_t count_register, GuestAddress stack_pointer)
{
    WriteU64(memory, stack_pointer - 8, destination_register);
    CopyForward(memory, static_cast<GuestAddress>(destination_register), source,
                static_cast<std::uint32_t>(count_register));
    return ReadU64(memory, stack_pointer - 8);
}

std::uint64_t MoveGuestMemory(GuestMemory& memory,
    std::uint64_t destination_register, GuestAddress source,
    std::uint64_t count_register, GuestAddress stack_pointer)
{
    const GuestAddress destination = static_cast<GuestAddress>(destination_register);
    if (destination == source)
        return destination_register;
    if (std::bit_cast<std::int32_t>(destination) <
        std::bit_cast<std::int32_t>(source))
        return CopyGuestMemory(memory, destination_register, source,
                               count_register, stack_pointer);

    const std::uint32_t bytes = static_cast<std::uint32_t>(count_register);
    CopyBackward(memory, destination, source, bytes);
    return destination_register + count_register - bytes;
}

} // namespace lo::semantic::gpu
