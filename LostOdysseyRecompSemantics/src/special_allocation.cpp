#include "lo_semantics/special_allocation.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kManagerGlobal = 0x8330b608u;

[[nodiscard]] std::int32_t Signed(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}

[[nodiscard]] GuestAddress Manager(GuestMemory& memory,
                                   SpecialAllocationServices& services)
{
    GuestAddress manager = memory.ReadU32(kManagerGlobal);
    if (manager == 0)
    {
        services.InitializeManager();
        manager = memory.ReadU32(kManagerGlobal);
    }
    return manager;
}

[[nodiscard]] std::uint32_t GrownCapacity(std::uint32_t new_count)
{
    const std::uint32_t triple = new_count + new_count * 2u;
    const auto eighth = static_cast<std::uint32_t>(Signed(triple) / 8);
    return new_count + eighth + 32u;
}

// The count was already written. Both the previous count and capacity are
// caller-cached values; the manager may mutate guest memory during resize.
void GrowEntriesIfNeeded(GuestMemory& memory, SpecialAllocationServices& services,
                         GuestAddress array, std::uint32_t new_count,
                         std::uint32_t previous_capacity)
{
    if (Signed(new_count) <= Signed(previous_capacity))
        return;

    const GuestAddress old_storage = memory.ReadU32(array);
    const std::uint32_t new_capacity = GrownCapacity(new_count);
    memory.WriteU32(array + 8, new_capacity);
    if (old_storage == 0 && Signed(new_capacity) == 0)
        return;

    const GuestAddress manager = Manager(memory, services);
    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 8) & ~3u;
    const GuestAddress resized = services.ResizeStorage(method, manager,
        old_storage, new_capacity * 8u, 8);
    memory.WriteU32(array, resized);
}

void CopyEntry(GuestMemory& memory, GuestAddress destination,
               GuestAddress source)
{
    if (destination == 0)
        return;
    memory.WriteU32(destination, memory.ReadU32(source));
    memory.WriteU32(destination + 4, memory.ReadU32(source + 4));
}
} // namespace

GuestAddress AllocateSpecialBlock(GuestMemory& memory,
                                  SpecialAllocationServices& services,
                                  GuestAddress allocator,
                                  std::uint32_t requested_bytes,
                                  GuestAddress frame_base)
{
    const GuestAddress free_array = allocator + 12;
    std::uint32_t free_index = memory.ReadU32(allocator + 16) - 1u;
    const std::uint32_t call_count = memory.ReadU32(allocator + 24);
    memory.WriteU32(allocator + 24, call_count + 1u);

    if (Signed(free_index) >= 0)
    {
        GuestAddress free_entry = memory.ReadU32(free_array) + free_index * 8u;
        while (Signed(free_index) >= 0)
        {
            if (memory.ReadU32(free_entry + 4) == requested_bytes)
            {
                const std::uint32_t active_count = memory.ReadU32(allocator + 4);
                const std::uint32_t active_capacity = memory.ReadU32(allocator + 8);
                const std::uint32_t new_count = active_count + 1u;
                const GuestAddress recycled_address = memory.ReadU32(free_entry);
                memory.WriteU32(allocator + 4, new_count);
                GrowEntriesIfNeeded(memory, services, allocator,
                                    new_count, active_capacity);

                const GuestAddress active_entry = memory.ReadU32(allocator) +
                                                  active_count * 8u;
                CopyEntry(memory, active_entry, free_entry);
                RemoveArrayRange(memory, services, free_array, free_index,
                                 1, 8, 8, frame_base - 128u);
                if (recycled_address != 0)
                    return recycled_address;
                break;
            }
            --free_index;
            free_entry -= 8;
        }
    }

    const GuestAddress manager = Manager(memory, services);
    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 16) & ~3u;
    const std::uint32_t rounded_bytes = (requested_bytes + 127u) & ~127u;
    const GuestAddress allocated_address = services.AllocateStorage(method,
        manager, rounded_bytes, 0);

    const std::uint32_t active_count = memory.ReadU32(allocator + 4);
    const std::uint32_t active_capacity = memory.ReadU32(allocator + 8);
    const std::uint32_t new_count = active_count + 1u;
    memory.WriteU32(frame_base + 84, requested_bytes);
    memory.WriteU32(frame_base + 80, allocated_address);
    memory.WriteU32(allocator + 4, new_count);
    GrowEntriesIfNeeded(memory, services, allocator, new_count, active_capacity);

    const GuestAddress active_entry = memory.ReadU32(allocator) +
                                      active_count * 8u;
    if (active_entry != 0)
    {
        // The original ld/std pair reads both halves before writing either.
        const std::uint32_t address_field = memory.ReadU32(frame_base + 80);
        const std::uint32_t size_field = memory.ReadU32(frame_base + 84);
        memory.WriteU32(active_entry, address_field);
        memory.WriteU32(active_entry + 4, size_field);
    }
    return allocated_address;
}

void FreeSpecialBlock(GuestMemory& memory, SpecialAllocationServices& services,
                      GuestAddress allocator, GuestAddress address,
                      GuestAddress frame_base)
{
    const std::uint32_t call_count = memory.ReadU32(allocator + 28);
    const std::uint32_t initial_count = memory.ReadU32(allocator + 4);
    memory.WriteU32(allocator + 28, call_count + 1u);
    if (Signed(initial_count) <= 0)
        return;

    GuestAddress active_entry = memory.ReadU32(allocator);
    std::uint32_t index = 0;
    for (;;)
    {
        if (memory.ReadU32(active_entry) == address)
            break;
        const std::uint32_t count = memory.ReadU32(allocator + 4);
        ++index;
        active_entry += 8;
        if (Signed(index) >= Signed(count))
            return;
    }

    const GuestAddress free_array = allocator + 12;
    const std::uint32_t free_count = memory.ReadU32(free_array + 4);
    const std::uint32_t free_capacity = memory.ReadU32(free_array + 8);
    const std::uint32_t new_count = free_count + 1u;
    memory.WriteU32(free_array + 4, new_count);
    GrowEntriesIfNeeded(memory, services, free_array,
                        new_count, free_capacity);

    const GuestAddress free_entry = memory.ReadU32(free_array) + free_count * 8u;
    CopyEntry(memory, free_entry, active_entry);
    RemoveArrayRange(memory, services, allocator, index, 1, 8, 8,
                     frame_base - 128u);
}

} // namespace lo::semantic::gpu
