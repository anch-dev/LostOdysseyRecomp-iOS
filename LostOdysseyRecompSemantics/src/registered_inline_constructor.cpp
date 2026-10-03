#include "lo_semantics/registered_inline_constructor.h"

#include "lo_semantics/manager_init.h"
#include "lo_semantics/object_registration.h"

namespace lo::semantic::gpu
{

std::uint64_t ConstructInlineManagedRegisteredObject(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t owner_register,
    GuestAddress caller_sp)
{
    constexpr GuestAddress manager_global = 0x8330b608u;
    const GuestAddress frame = caller_sp - 160u;
    GuestAddress manager = memory.ReadU32(manager_global);
    if (manager == 0)
    {
        (void)InitializeManager(memory, services, frame - 112u);
        manager = memory.ReadU32(manager_global);
    }

    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 4u) & ~3u;
    const std::uint64_t allocation = services.AllocateStorage(method,
        manager, 376, 8);
    memory.WriteU32(frame + 112u, static_cast<GuestAddress>(allocation));
    if (static_cast<GuestAddress>(allocation) == 0)
        return 0;

    // Keep the original outgoing store order. The initializer reads these
    // words live, so an object overlapping the stack must see the same values.
    memory.WriteU32(frame + 92u, 0x827ce228u);
    memory.WriteU32(frame + 80u, 0x04084084u);
    memory.WriteU32(frame + 84u, 0x00004000u);
    memory.WriteU32(frame + 108u, 0x822d3068u);
    memory.WriteU32(frame + 100u, 0x822d3068u);
    return InitializeRegisteredObject(memory, RegisteredObjectInput{
        allocation, 76, 1, 0, 0xffffffff82021f52ull, owner_register,
        0xffffffff8218c21cull, frame});
}

} // namespace lo::semantic::gpu
