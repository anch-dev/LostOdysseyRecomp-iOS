#include "lo_semantics/registered_getter_family.h"
#include "lo_semantics/registered_inline_constructor.h"

namespace lo::semantic::gpu::registered_getter_family
{
namespace
{
struct LibraryCalls
{
    GuestMemory& memory;
    ManagerFacadeServices& manager;
    registered_constructor_family::RegistrationServices& registration;
    GuestAddress frame;

    GuestAddress Global(const Singleton& entry) const { return entry.global; }
    void LoadInitial(std::uint32_t) const {}
    void LoadResult(std::uint32_t) const {}

    std::uint64_t Construct(const Singleton& entry)
    {
        if (entry.constructor == 0x827ce240u)
            return ConstructInlineManagedRegisteredObject(memory, manager,
                entry.owner, frame);
        std::uint64_t constructed = 0;
        (void)registered_constructor_family::Apply(entry.constructor, memory,
            manager, registration, entry.owner, frame, constructed);
        return constructed;
    }

    void Register(const Singleton& entry, std::uint64_t constructed)
    {
        (void)registration.Register(entry.registration, constructed, frame);
    }
};
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    std::uint64_t /* incoming_r3 */, GuestAddress caller_sp, std::uint64_t& result)
{
    const auto* entry = Find(address);
    if (entry == nullptr) return false;
    LibraryCalls calls{memory, manager_services, registration_services, caller_sp - 96u};
    result = FetchWith(*entry, memory, calls);
    return true;
}

} // namespace lo::semantic::gpu::registered_getter_family
