#include "lo_semantics/allocation_array.h"

#include "lo_semantics/memory_move.h"

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
} // namespace

void ResizeArray(GuestMemory& memory, ArrayResizeServices& services,
                 GuestAddress array, std::uint32_t element_size,
                 std::uint32_t argument)
{
    const GuestAddress old_storage = memory.ReadU32(array);
    if (old_storage == 0 && Signed(memory.ReadU32(array + 8)) == 0)
        return;

    const std::uint32_t bytes = memory.ReadU32(array + 8) * element_size;
    GuestAddress manager = memory.ReadU32(kManagerGlobal);
    if (manager == 0)
    {
        services.InitializeManager();
        manager = memory.ReadU32(kManagerGlobal);
    }

    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 8) & ~3u;
    const GuestAddress resized = services.ResizeStorage(method, manager, old_storage,
                                                         bytes, argument);
    memory.WriteU32(array, resized);
}

void RemoveArrayRange(GuestMemory& memory, ArrayResizeServices& services,
                      GuestAddress array, std::uint32_t first,
                      std::uint32_t count, std::uint32_t element_size,
                      std::uint32_t argument, GuestAddress frame_base)
{
    const std::uint32_t after_removed = first + count;
    const std::uint64_t destination_offset = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(Signed(first)) * Signed(element_size));
    const std::uint32_t old_count = memory.ReadU32(array + 4);
    const GuestAddress storage = memory.ReadU32(array);
    const std::uint32_t tail_count = old_count - first - count;
    const std::uint32_t source_offset = after_removed * element_size;
    const std::uint64_t destination_register =
        static_cast<std::uint64_t>(storage) + destination_offset;
    const GuestAddress source = storage + source_offset;
    const std::uint64_t tail_bytes = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(Signed(tail_count)) * Signed(element_size));
    (void)MoveGuestMemory(memory, destination_register, source,
                          tail_bytes, frame_base);

    // The move may touch the array header; the PPC body reloads both fields.
    const std::uint32_t new_count = memory.ReadU32(array + 4) - count;
    const std::uint32_t capacity = memory.ReadU32(array + 8);
    const std::uint32_t three_times_count = new_count + new_count * 2u;
    const std::uint32_t twice_capacity = capacity * 2u;
    memory.WriteU32(array + 4, new_count);

    const bool consider_shrink = Signed(three_times_count) < Signed(twice_capacity) ||
        Signed((capacity - new_count) * element_size) >= 16384;
    if (consider_shrink && (Signed(capacity - new_count) > 64 || Signed(new_count) == 0))
    {
        memory.WriteU32(array + 8, new_count);
        ResizeArray(memory, services, array, element_size, argument);
    }
}

} // namespace lo::semantic::gpu
