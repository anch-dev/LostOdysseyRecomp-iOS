#include "lo_semantics/manager_object_registration.h"

#include "lo_semantics/registered_callback_family.h"
#include "lo_semantics/registered_constructor_family.h"

#include <stdexcept>

namespace lo::semantic::gpu::manager_object_registration
{
namespace
{
constexpr GuestAddress kSharedObject = 0x83315f7cu;
constexpr std::uint64_t kDescriptor = 0xffffffff8218c210ull;

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    const std::uint64_t high = memory.ReadU32(address);
    return (high << 32) | memory.ReadU32(address + 4u);
}

// Existing lower-family APIs carry guest addresses. All their call sites are
// nested within this caller's bounded stack, so reconstruct the same high
// half from the signed low-word displacement before exposing external calls.
std::uint64_t FullStack(std::uint64_t parent_sp, GuestAddress nested_sp)
{
    const auto displacement = static_cast<std::int32_t>(
        nested_sp - static_cast<GuestAddress>(parent_sp));
    return parent_sp + static_cast<std::int64_t>(displacement);
}

class LowerServices final : public registered_constructor_family::RegistrationServices,
    public registered_callback_family::Services
{
public:
    LowerServices(manager_object_registration::Services& outer,
        std::uint64_t caller_sp)
        : outer_(outer), caller_sp_(caller_sp) {}

    std::uint64_t Register(GuestAddress target, std::uint64_t r3,
        GuestAddress sp) override
    { return outer_.Register(target, r3, FullStack(caller_sp_, sp)); }

    std::uint64_t CallExternalGetter(GuestAddress target, std::uint64_t r3,
        GuestAddress sp) override
    { return outer_.CallExternalGetter(target, r3, FullStack(caller_sp_, sp)); }

    std::uint64_t CallExternalRegistration(GuestAddress target,
        std::uint64_t r3, GuestAddress sp) override
    { return outer_.CallExternalRegistration(target, r3, FullStack(caller_sp_, sp)); }

    std::uint64_t CallReadyMethod(GuestAddress method, std::uint64_t receiver,
        GuestAddress sp) override
    { return outer_.CallReadyMethod(method, receiver, FullStack(caller_sp_, sp)); }

private:
    manager_object_registration::Services& outer_;
    std::uint64_t caller_sp_;
};
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services, Services& services,
    std::uint64_t /*incoming_r3*/, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != 0x82376ee8u)
        return false;

    const GuestAddress stack = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(stack - 8u, static_cast<std::uint32_t>(frame.lr));
    WriteU64(memory, stack - 16u, frame.r31);
    memory.WriteU32(stack - 96u, stack);

    const GuestAddress nested_sp = stack - 96u;
    result = memory.ReadU32(kSharedObject);
    if (static_cast<GuestAddress>(result) == 0)
    {
        LowerServices lower(services, caller_sp);
        std::uint64_t constructed = 0;
        if (!registered_constructor_family::Apply(0x82408438u, memory,
                manager_services, lower, kDescriptor, nested_sp, constructed))
            throw std::logic_error("recovered constructor mapping is missing");
        memory.WriteU32(kSharedObject, static_cast<GuestAddress>(constructed));
        (void)registered_callback_family::RegisterSecondaryObject(memory,
            manager_services, lower, constructed, nested_sp);
        result = memory.ReadU32(kSharedObject);
    }

    frame.lr = memory.ReadU32(stack - 8u);
    frame.r31 = ReadU64(memory, stack - 16u);
    return true;
}

Utf16CopyResult CopyUtf16Padded(GuestMemory& memory,
    std::uint64_t destination_r3, std::uint64_t source_r4,
    std::uint64_t count_r5)
{
    std::uint64_t write_pointer = destination_r3;
    if (static_cast<std::uint32_t>(count_r5) == 0)
        return {destination_r3, source_r4, count_r5};

    for (;;)
    {
        const std::uint16_t unit = memory.ReadU16(static_cast<GuestAddress>(source_r4));
        source_r4 += 2u;
        memory.WriteU16(static_cast<GuestAddress>(write_pointer), unit);
        write_pointer += 2u;
        if (unit == 0)
            break;
        --count_r5;
        if (static_cast<std::uint32_t>(count_r5) == 0)
            break;
    }

    if (static_cast<std::uint32_t>(count_r5) != 0)
    {
        // The terminating code unit was already stored. Keep the original
        // full r5 live while filling the remaining low-word count minus one.
        const std::uint32_t padding = static_cast<std::uint32_t>(count_r5 - 1u);
        for (std::uint32_t index = 0; index < padding; ++index)
        {
            memory.WriteU16(static_cast<GuestAddress>(write_pointer), 0);
            write_pointer += 2u;
        }
    }
    return {destination_r3, source_r4, count_r5};
}

} // namespace lo::semantic::gpu::manager_object_registration
