#include "lo_semantics/query_pool.h"

#include <bit>

namespace lo::semantic::gpu
{
namespace
{

constexpr std::uint32_t kPoolDataFreeFlags = 0xb1800000;
constexpr std::uint32_t kObjectFreeFlags = 0x24800000;

void ClearSlotBit(GuestMemory& memory, GuestAddress pool, GuestAddress slot)
{
    const GuestAddress pool_data = memory.ReadU32(pool + 8);
    const std::uint32_t difference = slot - pool_data;

    // 823CDCFC..823CDD20: srawi by 2, then by 4, then addze using
    // the carry from the second shift. A single signed / 64 changes
    // the result for negative, non-aligned differences.
    const std::int32_t quarter = std::bit_cast<std::int32_t>(difference) >> 2;
    const std::int32_t sixteenth = quarter >> 4;
    const bool carry = quarter < 0 && (static_cast<std::uint32_t>(quarter) & 0xf) != 0;
    const std::uint32_t bit = static_cast<std::uint32_t>(sixteenth + (carry ? 1 : 0));
    const std::uint32_t byte_offset = bit >> 3;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << (bit & 7));

    const std::uint8_t existing = memory.ReadU8(pool + byte_offset);
    memory.WriteU8(pool + byte_offset, static_cast<std::uint8_t>(existing & ~mask));
}

void UnlinkEmptyPool(GuestMemory& memory, QueryPoolServices& services,
    GuestAddress device, GuestAddress empty_pool)
{
    GuestAddress previous = device + 0x5424;
    if (previous == empty_pool)
        return;

    // The device list is followed exactly as 823CDD54..823CDDB0. The
    // candidate and next link are read anew after each service callback.
    for (;;)
    {
        if (memory.ReadU32(previous + 12) == 0)
            return;
        const GuestAddress candidate = memory.ReadU32(previous + 12);
        if (candidate != empty_pool)
        {
            previous = candidate;
            continue;
        }

        const std::uint32_t encoded = memory.ReadU32(candidate + 8);
        const std::uint32_t high = (std::rotl(encoded, 12) & 0xfff);
        const std::uint32_t bank = (high + 0x200) & 0x1000;
        const GuestAddress begin = ((encoded & 0x1fffffff) + bank) - 0x40000000;
        services.NotifyRange(begin, begin + 0x1000, 0);

        services.Free(memory.ReadU32(candidate + 8), kPoolDataFreeFlags);
        const GuestAddress next = memory.ReadU32(candidate + 12);
        memory.WriteU32(previous + 12, next);
        services.Free(candidate, kObjectFreeFlags);
        // The PPC resumes its list scan from the same predecessor, even after
        // a match. Callbacks can change which nodes follow it.
        continue;
    }
}

} // namespace

std::uint32_t ReleaseQuery(GuestMemory& memory, QueryPoolServices& services, GuestAddress query)
{
    const std::uint32_t reference_count = memory.ReadU32(query + 12);
    const GuestAddress device = memory.ReadU32(query);
    const std::uint32_t remaining = reference_count - 1;
    memory.WriteU32(query + 12, remaining);
    if (remaining != 0)
        return remaining;

    const std::uint32_t type = memory.ReadU32(query + 4);
    if (type == 9)
    {
        if (memory.ReadU32(query + 0x98) != 0)
        {
            std::uint32_t index = 0;
            GuestAddress owner_slot = query + 0x58;
            for (;;)
            {
                const GuestAddress pool = memory.ReadU32(owner_slot);
                const GuestAddress slot = memory.ReadU32(owner_slot - 0x3c);
                ClearSlotBit(memory, pool, slot);

                const GuestAddress current = memory.ReadU32(owner_slot);
                if (memory.ReadU32(current) == 0)
                {
                    const GuestAddress empty_candidate = memory.ReadU32(owner_slot);
                    if (memory.ReadU32(empty_candidate + 4) == 0)
                        UnlinkEmptyPool(memory, services, device, empty_candidate);
                }

                const std::uint32_t count = memory.ReadU32(query + 0x98);
                ++index;
                owner_slot += 4;
                if (index >= count)
                    break;
            }
        }
    }
    else if (type == 10)
    {
        const GuestAddress slot = memory.ReadU32(query + 0x1c);
        if (slot != 0)
        {
            const GuestAddress pool = memory.ReadU32(query + 0x58);
            ClearSlotBit(memory, pool, slot);
        }

        const GuestAddress pool = memory.ReadU32(query + 0x58);
        if (memory.ReadU32(pool) == 0 && memory.ReadU32(pool + 4) == 0)
            UnlinkEmptyPool(memory, services, device, pool);

        if ((memory.ReadU32(query + 0x10) & 0xff000000) == 0x02000000)
        {
            const GuestAddress flag = device + 0x2abf;
            memory.WriteU8(flag, static_cast<std::uint8_t>(memory.ReadU8(flag) & 0x7f));
        }
    }

    memory.WriteU32(query + 8, 0x78787878);
    services.Free(query, kObjectFreeFlags);
    return 0;
}

} // namespace lo::semantic::gpu
