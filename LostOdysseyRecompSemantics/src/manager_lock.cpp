#include "lo_semantics/manager_lock.h"

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kSectionGlobal = 0x83315fd4u;
constexpr GuestAddress kManagerGlobal = 0x83315fd8u;
constexpr GuestAddress kTryFailureCount = 0x83315fdcu;
} // namespace

std::uint64_t ReturnZeroStatus()
{
    return 0;
}

std::uint64_t StoreLockAndWaitForEnter(GuestMemory& memory,
    ManagerLockServices& services, std::uint64_t holder_register,
    std::uint64_t lock_register)
{
    memory.WriteU32(static_cast<GuestAddress>(holder_register),
                    static_cast<GuestAddress>(lock_register));
    const std::uint64_t section = lock_register + 4;
    std::uint64_t result = services.TryEnterCriticalSection(section);
    while (static_cast<GuestAddress>(result) == 0)
    {
        memory.WriteU32(kTryFailureCount,
                        memory.ReadU32(kTryFailureCount) + 1u);
        result = services.TryEnterCriticalSection(section);
    }
    return holder_register;
}

std::uint64_t InvokeManagerUnderLock(GuestMemory& memory,
    ManagerLockServices& services, GuestAddress frame_base)
{
    const GuestAddress section = memory.ReadU32(kSectionGlobal);
    (void)StoreLockAndWaitForEnter(memory, services,
                                   frame_base + 80u, section);

    const GuestAddress manager = memory.ReadU32(kManagerGlobal);
    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 56u) & ~GuestAddress{3};
    (void)services.CallMethod(method, manager);

    const GuestAddress held_lock = memory.ReadU32(frame_base + 80u);
    return services.LeaveCriticalSection(
        static_cast<std::uint64_t>(held_lock) + 4u);
}

} // namespace lo::semantic::gpu
