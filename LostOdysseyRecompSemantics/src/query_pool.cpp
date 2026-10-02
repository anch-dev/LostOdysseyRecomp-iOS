#include "lo_semantics/query_pool.h"

namespace lo::semantic::gpu
{

std::int32_t InitializeQuerySlot(GuestMemory& memory, QueryPoolServices& services,
                                 GuestAddress device, GuestAddress owner_slot,
                                 GuestAddress data_slot)
{
    constexpr std::int32_t out_of_memory = -2147024882; // 0x8007000e
    GuestAddress pool = device + 0x5424;

    memory.WriteU32(data_slot, 0);
    for (;;)
    {
        if (memory.ReadU32(pool) != 0xffffffff ||
            memory.ReadU32(pool + 4) != 0xffffffff)
            break;

        const GuestAddress next = memory.ReadU32(pool + 12);
        if (next != 0)
        {
            pool = next;
            continue;
        }

        const GuestAddress new_pool = services.Allocate(0x1780, 0x64800000);
        if (new_pool == 0)
            return out_of_memory;

        const GuestAddress data = services.Allocate(0x1000, 0xbc800000);
        memory.WriteU32(new_pool + 8, data);
        if (data == 0)
        {
            services.Free(new_pool, 0x24800000);
            return out_of_memory;
        }

        memory.WriteU32(pool + 12, new_pool);
        pool = new_pool;
        break;
    }

    // The source checks bytes 0..7, then reads byte 8 if every bitmap byte is
    // 0xff. Preserve that path: byte 8 belongs to the data-pointer field.
    std::uint32_t byte_index = 0;
    for (;;)
    {
        if (memory.ReadU8(pool + byte_index) != 0xff)
            break;
        ++byte_index;
        if (byte_index >= 8)
            break;
    }

    const std::uint8_t bitmap_byte = memory.ReadU8(pool + byte_index);
    std::uint32_t bit_index = 0;
    while ((bitmap_byte & (std::uint32_t{1} << bit_index)) != 0)
    {
        ++bit_index;
        if (bit_index >= 8)
            break;
    }

    memory.WriteU32(owner_slot, pool);
    const std::uint32_t slot_offset = ((byte_index << 3) + bit_index) << 6;
    const std::uint8_t previous_byte = memory.ReadU8(pool + byte_index);
    const std::uint8_t bit_mask = static_cast<std::uint8_t>(std::uint32_t{1} << bit_index);
    memory.WriteU8(pool + byte_index, static_cast<std::uint8_t>(previous_byte | bit_mask));
    const GuestAddress data = memory.ReadU32(pool + 8);
    memory.WriteU32(data_slot, data + slot_offset);
    return 0;
}

} // namespace lo::semantic::gpu
