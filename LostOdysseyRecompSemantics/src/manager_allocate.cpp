#include "lo_semantics/manager_allocate.h"

#include "lo_semantics/manager_lock.h"

#include <stdexcept>

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kSectionGlobal = 0x83315fd4u;
constexpr GuestAddress kManagerGlobal = 0x83315fd8u;

// StoreLockAndWaitForEnter owns the actual retry/counter semantics. Allocation
// callbacks use a different signature, so its unused method slot is adapted.
class LockServices final : public ManagerLockServices
{
public:
    explicit LockServices(ManagerAllocateServices& services)
        : services_(services) {}

    std::uint64_t TryEnterCriticalSection(
        std::uint64_t section_register) override
    {
        return services_.TryEnterCriticalSection(section_register);
    }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    {
        throw std::logic_error("allocation lock adapter has no manager method");
    }
    std::uint64_t LeaveCriticalSection(
        std::uint64_t section_register) override
    {
        return services_.LeaveCriticalSection(section_register);
    }

private:
    ManagerAllocateServices& services_;
};
} // namespace

std::uint64_t AllocateThroughPrimaryManager(GuestMemory& memory,
    ManagerAllocateServices& services, std::uint64_t receiver_register,
    std::uint64_t arg4_register, std::uint64_t arg5_register)
{
    const GuestAddress receiver = static_cast<GuestAddress>(receiver_register);
    const GuestAddress vtable = memory.ReadU32(receiver);
    const GuestAddress method = memory.ReadU32(vtable + 4u) & ~GuestAddress{3};
    memory.WriteU32(receiver + 0x48de8u,
                    static_cast<GuestAddress>(arg5_register));
    memory.WriteU32(receiver + 0x48de4u, 1u);
    const std::uint64_t result = services.AllocateVirtual(
        method, receiver_register, arg4_register, 8u);
    memory.WriteU32(receiver + 0x48de4u, 0);
    memory.WriteU32(receiver + 0x48de8u, 0);
    return result;
}

std::uint64_t AllocateThroughFallbackManager(GuestMemory& memory,
    ManagerAllocateServices& services, GuestAddress frame_base,
    std::uint64_t arg4_register, std::uint64_t arg5_register)
{
    LockServices lock_services(services);
    const GuestAddress lock = memory.ReadU32(kSectionGlobal);
    (void)StoreLockAndWaitForEnter(memory, lock_services,
                                   static_cast<std::uint64_t>(frame_base) + 80u,
                                   lock);

    const GuestAddress manager = memory.ReadU32(kManagerGlobal);
    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 16u) & ~GuestAddress{3};
    const std::uint64_t result = services.AllocateVirtual(
        method, manager, arg4_register, arg5_register);
    const GuestAddress held_lock = memory.ReadU32(frame_base + 80u);
    (void)services.LeaveCriticalSection(
        static_cast<std::uint64_t>(held_lock) + 4u);
    return result;
}

} // namespace lo::semantic::gpu
