#include "lo_semantics/pointer_vector.h"

#include <bit>

namespace lo::semantic::gpu
{

std::uint64_t FlushPointerVector(GuestMemory& memory,
    ManagerLockServices& locks, FallbackResizeServices& manager,
    std::uint64_t vector_register, GuestAddress frame_base)
{
    constexpr GuestAddress section_global = 0x83315fd4u;
    constexpr GuestAddress manager_global = 0x83315fd8u;
    const auto vector = static_cast<GuestAddress>(vector_register);
    (void)StoreLockAndWaitForEnter(memory, locks, frame_base + 80u,
                                  memory.ReadU32(section_global));

    std::uint32_t index = 0;
    while (std::bit_cast<std::int32_t>(index) <
           std::bit_cast<std::int32_t>(memory.ReadU32(vector + 12u)))
    {
        // Each release may change the manager, backing pointer, or live count.
        const GuestAddress receiver = memory.ReadU32(manager_global);
        const GuestAddress backing = memory.ReadU32(vector);
        const GuestAddress vtable = memory.ReadU32(receiver);
        const GuestAddress pointer = memory.ReadU32(backing + index * 4u);
        const GuestAddress method = memory.ReadU32(vtable + 12u) & ~GuestAddress{3};
        (void)manager.ReleaseThroughManager(method, receiver, pointer);
        ++index;
    }
    memory.WriteU32(vector + 12u, 0);
    const GuestAddress held_lock = memory.ReadU32(frame_base + 80u);
    return locks.LeaveCriticalSection(static_cast<std::uint64_t>(held_lock) + 4u);
}

} // namespace lo::semantic::gpu
