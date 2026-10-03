#pragma once

#include "lo_semantics/guest_memory.h"
#include "lo_semantics/manager_facade.h"

namespace lo::semantic::gpu
{

// Registers and caller-owned outgoing stack used by 82410A28. Its PPC frame
// begins at caller_sp - 112; stack arguments remain live guest-memory inputs.
struct RegisteredObjectInput
{
    std::uint64_t object_register = 0;      // r3
    std::uint64_t size_register = 0;        // r5
    std::uint64_t category_register = 0;    // r6
    std::uint64_t byte_register = 0;        // r7
    std::uint64_t descriptor_register = 0;  // r8
    std::uint64_t owner_register = 0;       // r9
    std::uint64_t tag_register = 0;         // r10
    GuestAddress caller_sp = 0;
};

// 8240CC58: initialize the common object header, using the live global list
// state. This helper preserves the full incoming r3 object register.
[[nodiscard]] std::uint64_t InitializeRegisteredObjectBase(
    GuestMemory& memory, std::uint64_t object_register,
    std::uint64_t size_register, std::uint64_t descriptor_register,
    std::uint64_t owner_register, std::uint64_t flags_register);

// 82410A28: extend that common header with the 376-byte registered layout.
[[nodiscard]] std::uint64_t InitializeRegisteredObject(
    GuestMemory& memory, const RegisteredObjectInput& input);

// 82410B90: allocate 376 bytes through the recovered manager facade and
// initialize the object. caller_sp is the guest stack at entry.
[[nodiscard]] std::uint64_t ConstructRegisteredObject(GuestMemory& memory,
    ManagerFacadeServices& services, std::uint64_t owner_register,
    GuestAddress caller_sp);

// Direct singleton construction and secondary registration are still guest
// boundaries. Their callbacks may change globals and return full r3 values.
class ObjectRegistrationServices
{
public:
    virtual ~ObjectRegistrationServices() = default;
    virtual std::uint64_t GetPrimaryObject() = 0; // 824059D8
    virtual std::uint64_t CreateSecondary(std::uint64_t descriptor) = 0; // 82408438
    virtual std::uint64_t RegisterSecondary(
        std::uint64_t incoming_register) = 0; // 824084F0
    virtual std::uint64_t CallReadyMethod(GuestAddress method,
        std::uint64_t receiver_register) = 0; // Primary vtable +124
};

// 82410C48: link primary and secondary instances, initialize the secondary
// lazily, then invoke the primary ready method when the global gate is set.
[[nodiscard]] std::uint64_t RegisterObjectGraph(GuestMemory& memory,
    ObjectRegistrationServices& services);

} // namespace lo::semantic::gpu
