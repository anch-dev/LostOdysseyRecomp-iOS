#include "lo_semantics/manager_resize.h"

#include "lo_semantics/memory_move.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu
{
namespace
{
[[nodiscard]] std::int32_t Signed(std::uint32_t value)
{
    return std::bit_cast<std::int32_t>(value);
}

[[nodiscard]] GuestAddress Method(GuestMemory& memory,
    std::uint64_t manager_register, std::uint32_t slot)
{
    const GuestAddress vtable = memory.ReadU32(
        static_cast<GuestAddress>(manager_register));
    return memory.ReadU32(vtable + slot) & ~3u;
}

[[nodiscard]] std::uint64_t Allocate(GuestMemory& memory,
    ManagerResizeServices& services, std::uint64_t manager_register,
    std::uint64_t bytes_register, std::uint64_t flags_register)
{
    return services.AllocateStorage(Method(memory, manager_register, 4),
        manager_register, bytes_register, flags_register);
}

void Free(GuestMemory& memory, ManagerResizeServices& services,
    std::uint64_t manager_register, std::uint64_t address_register)
{
    services.FreeStorage(Method(memory, manager_register, 12),
        manager_register, address_register);
}

[[nodiscard]] std::uint64_t CopyCount(std::uint32_t available,
    std::uint64_t requested_register)
{
    const std::uint32_t difference = available -
        static_cast<std::uint32_t>(requested_register);
    return Signed(difference) < 0 ? available : requested_register;
}
} // namespace

std::uint64_t FindPrimaryResizeNode(GuestMemory& memory,
    std::uint64_t manager_register, std::uint64_t requested_register,
    std::uint64_t mode_register, std::uint64_t group_register)
{
    const GuestAddress manager = static_cast<GuestAddress>(manager_register);
    const std::uint32_t requested = static_cast<std::uint32_t>(requested_register);
    if (Signed(requested) >= 32769)
        return manager_register + 3364u;

    const std::uint32_t lookup_offset = (requested + 878u) * 4u;
    const std::uint32_t selected = memory.ReadU32(manager + lookup_offset);
    if (static_cast<std::uint32_t>(mode_register) != 0)
    {
        const std::uint32_t group = static_cast<std::uint32_t>(group_register);
        const std::uint32_t entry = selected + group * 42u;
        return manager_register + entry * 20u + 4u;
    }
    return manager_register + selected * 20u + 2524u;
}

std::uint64_t ResizePrimaryManagerStorage(GuestMemory& memory,
    ManagerResizeServices& services, std::uint64_t manager_register,
    std::uint64_t old_storage_register, std::uint64_t new_bytes_register,
    std::uint64_t flags_register, GuestAddress frame_base)
{
    const GuestAddress manager = static_cast<GuestAddress>(manager_register);
    const GuestAddress old_storage = static_cast<GuestAddress>(old_storage_register);
    const std::uint32_t new_bytes = static_cast<std::uint32_t>(new_bytes_register);

    if (old_storage == 0)
        return Allocate(memory, services, manager_register,
                        new_bytes_register, flags_register);
    if (new_bytes == 0)
    {
        Free(memory, services, manager_register, old_storage_register);
        return 0;
    }

    const std::uint32_t pointer_high = std::rotl(old_storage, 5) & 0x1fu;
    const std::uint32_t pointer_middle = std::rotl(old_storage, 21) & 0xffe0u;
    const GuestAddress sentinel = manager + 3364u;
    const std::uint32_t lookup_offset = (pointer_high + 846u) * 4u;
    const GuestAddress list = memory.ReadU32(manager + lookup_offset) +
                              pointer_middle;
    const GuestAddress node = memory.ReadU32(list + 12u);

    if (node != sentinel)
    {
        const std::uint32_t available = memory.ReadU32(node + 16u);
        if (new_bytes <= available)
        {
            const std::uint32_t group = memory.ReadU32(node + 4u);
            const std::uint32_t mode = memory.ReadU32(node);
            const std::uint64_t found = FindPrimaryResizeNode(memory,
                manager_register, new_bytes_register, mode, group);
            if (static_cast<GuestAddress>(found) == node)
                return old_storage_register;
        }

        const std::uint64_t allocated = Allocate(memory, services,
            manager_register, new_bytes_register, flags_register);
        // The allocator may change the list and the selected node.
        const GuestAddress current_node = memory.ReadU32(list + 12u);
        const std::uint32_t copy_capacity = memory.ReadU32(current_node + 16u);
        (void)CopyGuestMemory(memory, allocated, old_storage,
            CopyCount(copy_capacity, new_bytes_register), frame_base);
        Free(memory, services, manager_register, old_storage_register);
        return allocated;
    }

    const std::uint32_t current_size = memory.ReadU32(list + 4u);
    if (new_bytes <= current_size)
    {
        const std::uint32_t twice_size = current_size * 2u;
        const std::uint32_t triple_requested = new_bytes + new_bytes * 2u;
        if (triple_requested >= twice_size)
        {
            memory.WriteU32(list, new_bytes);
            return old_storage_register;
        }
    }

    const std::uint64_t allocated = Allocate(memory, services,
        manager_register, new_bytes_register, flags_register);
    const std::uint32_t copy_capacity = memory.ReadU32(list);
    (void)CopyGuestMemory(memory, allocated, old_storage,
        CopyCount(copy_capacity, new_bytes_register), frame_base);
    Free(memory, services, manager_register, old_storage_register);
    return allocated;
}

} // namespace lo::semantic::gpu
