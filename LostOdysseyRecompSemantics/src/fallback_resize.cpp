#include "lo_semantics/fallback_resize.h"

#include "lo_semantics/memory_move.h"

#include <bit>

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kSectionGlobal = 0x83315fd4u;
constexpr GuestAddress kManagerGlobal = 0x83315fd8u;

[[nodiscard]] GuestAddress ReadAddress(GuestMemory& memory, GuestAddress address)
{
    return memory.ReadU32(address);
}

[[nodiscard]] GuestAddress ClassForRequestedSize(
    GuestMemory& memory, GuestAddress pool, GuestAddress requested)
{
    if (requested > memory.ReadU32(pool + 108u) ||
        requested < 16u || requested >= 512u)
        return 0;

    for (std::uint32_t index = 1; index < 6; ++index)
    {
        if (memory.ReadU32(pool + 24u + 20u * (index - 1u)) >= requested)
            return pool + 20u * index;
    }
    return 0;
}

[[nodiscard]] GuestAddress PopClassPointer(
    GuestMemory& memory, GuestAddress descriptor)
{
    if (descriptor == 0)
        return 0;
    const std::uint32_t count = memory.ReadU32(descriptor + 12u);
    if (count == 0)
        return 0;
    const std::uint32_t next = count - 1u;
    const GuestAddress backing = memory.ReadU32(descriptor);
    memory.WriteU32(descriptor + 12u, next);
    return memory.ReadU32(backing + next * 4u);
}

[[nodiscard]] GuestAddress ClassForAllocationSize(
    GuestMemory& memory, GuestAddress pool, GuestAddress allocation_size)
{
    for (std::uint32_t index = 1; index < 6; ++index)
    {
        const GuestAddress descriptor = pool + 20u * index;
        const std::uint32_t lower = memory.ReadU32(descriptor + 4u);
        if (lower > allocation_size)
            continue;
        const std::uint32_t upper = memory.ReadU32(descriptor + 8u);
        if (upper > allocation_size)
            return descriptor;
    }
    return allocation_size < 1024u ? pool : 0u;
}

[[nodiscard]] GuestAddress AllocationSizeLookup(
    GuestMemory& memory, GuestAddress old_pointer)
{
    const std::uint32_t table_index = std::rotl(old_pointer, 5) & 31u;
    const std::uint32_t entry_offset = std::rotl(old_pointer, 21) & 0xffe0u;
    const GuestAddress manager = ReadAddress(memory, kManagerGlobal);
    const GuestAddress table = ReadAddress(
        memory, manager + (table_index + 846u) * 4u);
    return ReadAddress(memory, table + entry_offset);
}

void ReleaseThroughManager(GuestMemory& memory,
    ManagerLockServices& lock_services, FallbackResizeServices& resize_services,
    std::uint64_t old_register, GuestAddress frame_base)
{
    (void)StoreLockAndWaitForEnter(memory, lock_services, frame_base + 80u,
                                   ReadAddress(memory, kSectionGlobal));
    const GuestAddress manager = ReadAddress(memory, kManagerGlobal);
    const GuestAddress vtable = ReadAddress(memory, manager);
    const GuestAddress method = ReadAddress(memory, vtable + 12u) & ~GuestAddress{3};
    (void)resize_services.ReleaseThroughManager(method, manager, old_register);
    const GuestAddress held_lock = ReadAddress(memory, frame_base + 80u);
    (void)lock_services.LeaveCriticalSection(
        static_cast<std::uint64_t>(held_lock) + 4u);
}

[[nodiscard]] std::uint64_t ReallocateThroughManager(
    GuestMemory& memory, ManagerLockServices& lock_services,
    FallbackResizeServices& resize_services,
    std::uint64_t old_register, std::uint64_t size_register,
    std::uint64_t argument_register, GuestAddress frame_base)
{
    (void)StoreLockAndWaitForEnter(memory, lock_services, frame_base + 80u,
                                   ReadAddress(memory, kSectionGlobal));
    const GuestAddress manager = ReadAddress(memory, kManagerGlobal);
    const GuestAddress vtable = ReadAddress(memory, manager);
    const GuestAddress method = ReadAddress(memory, vtable + 8u) & ~GuestAddress{3};
    const std::uint64_t result = resize_services.ReallocateThroughManager(
        method, manager, old_register, size_register, argument_register);
    const GuestAddress held_lock = ReadAddress(memory, frame_base + 80u);
    (void)lock_services.LeaveCriticalSection(
        static_cast<std::uint64_t>(held_lock) + 4u);
    return result;
}

} // namespace

std::uint64_t AppendPointerToVector(
    GuestMemory& memory, FallbackResizeServices& services,
    std::uint64_t vector_register, std::uint64_t value_register)
{
    const GuestAddress vector = static_cast<GuestAddress>(vector_register);
    std::uint64_t result = vector_register;
    const auto count = memory.ReadU32(vector + 12u);
    const auto capacity = memory.ReadU32(vector + 16u);
    if (std::bit_cast<std::int32_t>(count) >=
        std::bit_cast<std::int32_t>(capacity))
        result = services.GrowPointerVector(vector_register, value_register);

    const std::uint32_t current = memory.ReadU32(vector + 12u);
    const GuestAddress backing = memory.ReadU32(vector);
    memory.WriteU32(backing + current * 4u,
                    static_cast<GuestAddress>(value_register));
    memory.WriteU32(vector + 12u, current + 1u);
    return result;
}

std::uint64_t ResizeFallbackAllocation(
    GuestMemory& memory, ManagerLockServices& lock_services,
    FallbackResizeServices& resize_services,
    std::uint64_t old_register, std::uint64_t size_register,
    std::uint64_t argument_register, GuestAddress r13,
    GuestAddress frame_base)
{
    const GuestAddress thread_state = ReadAddress(memory, r13);
    const GuestAddress pool = ReadAddress(memory, thread_state + 4u);
    const GuestAddress old_pointer = static_cast<GuestAddress>(old_register);
    const GuestAddress requested = static_cast<GuestAddress>(size_register);

    if (pool == 0)
        return ReallocateThroughManager(memory, lock_services, resize_services,
            old_register, size_register, argument_register, frame_base);

    if (old_pointer == 0)
    {
        if (requested != 0)
        {
            const GuestAddress descriptor =
                ClassForRequestedSize(memory, pool, requested);
            const GuestAddress pointer = PopClassPointer(memory, descriptor);
            if (pointer != 0)
                return pointer;
        }
        return ReallocateThroughManager(memory, lock_services, resize_services,
            old_register, size_register, argument_register, frame_base);
    }

    const GuestAddress old_size = AllocationSizeLookup(memory, old_pointer);
    if (old_size == 0xffffffffu)
        return ReallocateThroughManager(memory, lock_services, resize_services,
            old_register, size_register, argument_register, frame_base);

    if (requested == 0)
    {
        const GuestAddress descriptor =
            ClassForAllocationSize(memory, pool, old_size);
        if (descriptor != 0)
        {
            (void)AppendPointerToVector(memory, resize_services,
                                        descriptor, old_register);
            return 0;
        }
        ReleaseThroughManager(memory, lock_services, resize_services,
                              old_register, frame_base);
        return 0;
    }

    const GuestAddress descriptor = ClassForRequestedSize(memory, pool, requested);
    const GuestAddress replacement = PopClassPointer(memory, descriptor);
    if (replacement == 0)
        return ReallocateThroughManager(memory, lock_services, resize_services,
            old_register, size_register, argument_register, frame_base);

    const std::uint64_t difference =
        static_cast<std::uint64_t>(old_size) - size_register;
    const std::int32_t signed_low =
        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference));
    const std::uint64_t sign_mask = signed_low < 0 ? ~std::uint64_t{0} : 0;
    const std::uint64_t copy_count =
        size_register + (sign_mask & difference);
    (void)CopyGuestMemory(memory, replacement, old_pointer, copy_count, frame_base);

    const GuestAddress current_old_size =
        AllocationSizeLookup(memory, old_pointer);
    const GuestAddress old_descriptor =
        ClassForAllocationSize(memory, pool, current_old_size);
    if (old_descriptor != 0)
    {
        (void)AppendPointerToVector(memory, resize_services,
                                    old_descriptor, old_register);
        return replacement;
    }
    ReleaseThroughManager(memory, lock_services, resize_services,
                          old_register, frame_base);
    return replacement;
}

} // namespace lo::semantic::gpu
