#include "lo_semantics/object_registration.h"

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kRegistrationMode = 0x83315ed8u;
constexpr GuestAddress kObjectListHead = 0x83315ef0u;
constexpr GuestAddress kPrimaryObject = 0x83315f9cu;
constexpr GuestAddress kSecondaryObject = 0x83315f7cu;
constexpr GuestAddress kBaseVtable = 0x821915d0u;
constexpr GuestAddress kRegisteredVtable = 0x82005160u;
constexpr std::uint64_t kBaseFlags = 0x0400008000004000ull;

void ClearWords(GuestMemory& memory, GuestAddress object,
    std::uint32_t first, std::uint32_t last)
{
    for (std::uint32_t offset = first; offset <= last; offset += 4)
        memory.WriteU32(object + offset, 0);
}

} // namespace

std::uint64_t InitializeRegisteredObjectBase(GuestMemory& memory,
    std::uint64_t object_register, std::uint64_t size_register,
    std::uint64_t descriptor_register, std::uint64_t owner_register,
    std::uint64_t flags_register)
{
    const GuestAddress object = static_cast<GuestAddress>(object_register);
    memory.WriteU32(object, 0x8218d6f8u);
    memory.WriteU32(object + 16u, 0);
    memory.WriteU32(object + 24u, 0);
    memory.WriteU32(object + 4u, 0xffffffffu);
    memory.WriteU32(object + 32u, 0xffffffffu);
    memory.WriteU32(object + 28u, 0);
    const std::uint64_t flags = flags_register | kBaseFlags;
    memory.WriteU32(object + 8u, static_cast<std::uint32_t>(flags >> 32));
    memory.WriteU32(object + 12u, static_cast<std::uint32_t>(flags));
    memory.WriteU32(object + 44u, 0);
    const bool link_into_list = memory.ReadU32(kRegistrationMode) == 0;
    memory.WriteU32(object + 48u, 0);
    memory.WriteU32(object + 52u, 0);
    memory.WriteU32(object + 56u, 0);
    memory.WriteU32(object + 44u, static_cast<GuestAddress>(descriptor_register));
    memory.WriteU32(object + 40u, static_cast<GuestAddress>(owner_register));
    if (link_into_list)
    {
        memory.WriteU32(object + 32u, memory.ReadU32(kObjectListHead));
        memory.WriteU32(kObjectListHead, object);
    }
    ClearWords(memory, object, 64, 76);
    memory.WriteU32(object + 80u, static_cast<GuestAddress>(size_register));
    memory.WriteU32(object, kBaseVtable);
    ClearWords(memory, object, 84, 100);
    memory.WriteU32(object + 104u, 1);
    ClearWords(memory, object, 108, 136);
    return object_register;
}

std::uint64_t InitializeRegisteredObject(GuestMemory& memory,
    const RegisteredObjectInput& input)
{
    const GuestAddress frame = input.caller_sp - 112u;
    memory.WriteU8(frame + 167u, static_cast<std::uint8_t>(input.byte_register));
    const std::uint64_t base_flags =
        (std::uint64_t{memory.ReadU32(frame + 192u)} << 32) |
        memory.ReadU32(frame + 196u);
    memory.WriteU32(frame + 188u, static_cast<GuestAddress>(input.tag_register));
    const std::uint64_t object_register = InitializeRegisteredObjectBase(memory,
        input.object_register, input.size_register, input.descriptor_register,
        input.owner_register, base_flags);
    const GuestAddress object = static_cast<GuestAddress>(object_register);
    const std::uint32_t last_argument = memory.ReadU32(frame + 220u);
    memory.WriteU32(frame + 84u, kRegisteredVtable);
    memory.WriteU32(object + 140u, 0);
    memory.WriteU32(object + 144u, 0);
    memory.WriteU32(object + 148u, 0);
    memory.WriteU32(object + 152u, 0);
    memory.WriteU32(object + 156u, 0);
    memory.WriteU16(object + 160u, 0);
    memory.WriteU32(object + 164u, 0);
    memory.WriteU32(frame + 80u, 0);
    memory.WriteU32(object + 180u, 8);
    memory.WriteU32(object + 168u, 0);
    memory.WriteU32(object + 172u, 0);
    memory.WriteU32(object + 176u, 0);
    memory.WriteU32(object + 184u,
        static_cast<GuestAddress>(input.category_register | 128u));
    const std::uint8_t saved_byte = memory.ReadU8(frame + 167u);
    memory.WriteU32(object + 188u, 0);
    memory.WriteU32(object + 196u, 0);
    memory.WriteU8(object + 192u, saved_byte);
    const std::uint32_t argument_92 = memory.ReadU32(frame + 204u);
    memory.WriteU32(object, memory.ReadU32(frame + 84u));
    ClearWords(memory, object, 208, 252);
    const std::uint32_t argument_100 = memory.ReadU32(frame + 212u);
    ClearWords(memory, object, 256, 276);
    memory.WriteU32(object + 284u, argument_92);
    memory.WriteU32(object + 280u, 0);
    memory.WriteU32(object + 288u, argument_100);
    memory.WriteU32(object + 292u, last_argument);
    ClearWords(memory, object, 296, 308);
    memory.WriteU32(object + 312u, 8);
    ClearWords(memory, object, 316, 328);
    memory.WriteU32(object + 332u, 8);
    ClearWords(memory, object, 336, 344);
    memory.WriteU32(object + 348u, 1);
    ClearWords(memory, object, 352, 372);
    memory.WriteU32(object + 200u, memory.ReadU32(frame + 188u));
    return object_register;
}

std::uint64_t ConstructRegisteredObject(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t owner_register,
    GuestAddress caller_sp)
{
    const GuestAddress frame = caller_sp - 128u;
    const std::uint64_t allocation = AllocateManagerBuffer(memory, services,
        376, frame);
    if (static_cast<GuestAddress>(allocation) == 0)
        return 0;

    // These are outgoing PPC arguments in the caller's frame. The callee
    // reloads each at its original point, after base-header construction.
    memory.WriteU32(frame + 92u, 0x82412100u);
    memory.WriteU32(frame + 80u, 0x04084084u);
    memory.WriteU32(frame + 84u, 0x00004000u);
    memory.WriteU32(frame + 108u, 0x822d3068u);
    memory.WriteU32(frame + 100u, 0x8240f8c8u);
    return InitializeRegisteredObject(memory, RegisteredObjectInput{
        allocation, 376, 0x10000000u, 0,
        0xffffffff821913eeull, owner_register,
        0xffffffff8218c21cull, frame});
}

std::uint64_t RegisterObjectGraph(GuestMemory& memory,
    ObjectRegistrationServices& services)
{
    std::uint64_t result = services.GetPrimaryObject();
    GuestAddress primary = memory.ReadU32(kPrimaryObject);
    if (static_cast<GuestAddress>(result) != primary)
    {
        result = services.GetPrimaryObject();
        primary = memory.ReadU32(kPrimaryObject);
        memory.WriteU32(primary + 60u, static_cast<GuestAddress>(result));
    }
    else
        memory.WriteU32(primary + 60u, 0);

    GuestAddress secondary = memory.ReadU32(kSecondaryObject);
    if (secondary == 0)
    {
        result = services.CreateSecondary(0xffffffff8218c210ull);
        memory.WriteU32(kSecondaryObject, static_cast<GuestAddress>(result));
        result = services.RegisterSecondary(result);
        secondary = memory.ReadU32(kSecondaryObject);
    }

    const bool ready = memory.ReadU32(kRegistrationMode) != 0;
    primary = memory.ReadU32(kPrimaryObject);
    memory.WriteU32(primary + 196u, secondary);
    primary = memory.ReadU32(kPrimaryObject);
    memory.WriteU32(primary + 52u, primary);
    if (ready)
    {
        primary = memory.ReadU32(kPrimaryObject);
        if (memory.ReadU32(primary + 52u) == primary)
        {
            const GuestAddress vtable = memory.ReadU32(primary);
            const GuestAddress method = memory.ReadU32(vtable + 124u) & ~3u;
            result = services.CallReadyMethod(method, primary);
        }
    }
    return result;
}

} // namespace lo::semantic::gpu
