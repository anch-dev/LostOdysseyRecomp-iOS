#include "lo_semantics/instance_flag_initializer_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_flag_initializer_family
{
namespace
{

struct Spec
{
    GuestAddress address;
    GuestAddress vtable;
};

// The generator validates each complete generated PPC body and CFG.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE FLAG PARAMETERS
    {0x8268c9e8u, 0x821fad98u},
    {0x8268ca78u, 0x821faee0u},
    {0x8268cb98u, 0x821fab08u},
    {0x8268cd08u, 0x821f9630u},
    // END GENERATED INSTANCE FLAG PARAMETERS
};

} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result)
{
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& entry, GuestAddress target)
        { return entry.address < target; });
    if (spec == std::end(kSpecs) || spec->address != address)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    if (object != 0)
    {
        const std::uint32_t flags = memory.ReadU32(object + 120u);
        const std::uint32_t enabled_flags = flags | 0x80000000u;
        memory.WriteU32(object, spec->vtable);
        memory.WriteU32(object + 120u, enabled_flags);
    }
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_flag_initializer_family
