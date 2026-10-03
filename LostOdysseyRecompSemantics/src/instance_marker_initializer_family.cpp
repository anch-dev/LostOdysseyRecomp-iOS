#include "lo_semantics/instance_marker_initializer_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_marker_initializer_family
{
namespace
{

struct Spec
{
    GuestAddress address;
    GuestAddress vtable;
};

// Each full ten-instruction body is validated before emitting its vtable.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE MARKER PARAMETERS
    {0x824d5d80u, 0x821b58f8u},
    {0x824d6218u, 0x821b5278u},
    {0x824d6438u, 0x821b4bf8u},
    {0x824d64c8u, 0x821b45b8u},
    {0x824d69f0u, 0x821b4278u},
    {0x824d6f78u, 0x821b17c0u},
    {0x824d7ed8u, 0x821b3bf8u},
    {0x824d8120u, 0x821b55b8u},
    {0x824d8210u, 0x821b5c38u},
    {0x824d8810u, 0x821b4f38u},
    {0x82537230u, 0x821c72b0u},
    {0x825373f0u, 0x821c6f60u},
    {0x825482b8u, 0x821c8c98u},
    // END GENERATED INSTANCE MARKER PARAMETERS
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
        memory.WriteU32(object + 560u, 0u);
        memory.WriteU32(object + 564u, 0u);
        memory.WriteU8(object + 568u, 0u);
        memory.WriteU32(object, spec->vtable);
    }
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_marker_initializer_family
