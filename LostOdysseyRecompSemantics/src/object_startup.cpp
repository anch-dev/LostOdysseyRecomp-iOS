#include "lo_semantics/object_startup.h"

namespace lo::semantic::gpu
{
namespace
{
std::uint64_t ReadFlags(GuestMemory& memory, GuestAddress object)
{
    return (std::uint64_t{memory.ReadU32(object + 8u)} << 32) |
           memory.ReadU32(object + 12u);
}

void WriteFlags(GuestMemory& memory, GuestAddress object, std::uint64_t value)
{
    memory.WriteU32(object + 8u, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(object + 12u, static_cast<std::uint32_t>(value));
}

GuestAddress Method(GuestMemory& memory, GuestAddress object,
    GuestAddress slot)
{
    return memory.ReadU32(memory.ReadU32(object) + slot) & ~3u;
}
} // namespace

std::uint64_t PrepareObject(GuestMemory& memory,
    ObjectStartupServices& services, std::uint64_t object_register)
{
    const GuestAddress object = static_cast<GuestAddress>(object_register);
    if (memory.ReadU32(object + 4u) == 0xffffffffu)
        return 0;
    const std::uint64_t flags = ReadFlags(memory, object);
    if ((flags & 0x8000u) != 0)
        return 0;

    // The original obtains the method before storing the updated flags.
    const GuestAddress method = Method(memory, object, 32u);
    WriteFlags(memory, object, (flags & ~0x20000ull) | 0x8000u);
    (void)services.CallMethod(method, object_register);
    return 1;
}

std::uint64_t CompleteObjectStartup(GuestMemory& memory,
    ObjectStartupServices& services, std::uint64_t object_register)
{
    const GuestAddress object = static_cast<GuestAddress>(object_register);
    if (memory.ReadU32(object + 4u) == 0xffffffffu)
        return 0;
    const std::uint64_t flags = ReadFlags(memory, object);
    if ((flags & 0x10000u) != 0)
        return 0;

    const GuestAddress method = Method(memory, object, 40u);
    WriteFlags(memory, object, (flags & ~0x20ull) | 0x10000u);
    (void)services.CallMethod(method, object_register);
    return 1;
}

std::uint64_t StartObject(GuestMemory& memory,
    ObjectStartupServices& services, std::uint64_t object_register)
{
    const GuestAddress object = static_cast<GuestAddress>(object_register);
    if ((ReadFlags(memory, object) & 0x10000u) != 0)
        return object_register;

    (void)PrepareObject(memory, services, object_register);
    for (;;)
    {
        const GuestAddress method = Method(memory, object, 36u);
        const std::uint64_t result = services.CallMethod(method, object_register);
        if (static_cast<std::int32_t>(result) != 0)
            return CompleteObjectStartup(memory, services, object_register);
        (void)services.WaitForRetry(0);
    }
}

std::uint64_t GetPrimaryRegisteredObject(GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    ObjectRegistrationServices& registration_services,
    GuestAddress caller_sp)
{
    constexpr GuestAddress primary_global = 0x83315f9cu;
    GuestAddress primary = memory.ReadU32(primary_global);
    if (primary == 0)
    {
        const std::uint64_t constructed = ConstructRegisteredObject(memory,
            manager_services, 0xffffffff8218c210ull, caller_sp - 96u);
        memory.WriteU32(primary_global, static_cast<GuestAddress>(constructed));
        (void)RegisterObjectGraph(memory, registration_services);
        primary = memory.ReadU32(primary_global);
    }
    return primary;
}

} // namespace lo::semantic::gpu
