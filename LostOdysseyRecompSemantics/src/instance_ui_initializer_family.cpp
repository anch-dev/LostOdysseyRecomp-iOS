#include "lo_semantics/instance_ui_initializer_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_ui_initializer_family
{
namespace
{

// Complete generated PPC bodies are checked before emitting parameters.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE UI PARAMETERS
    {0x82631ad0u, 656u, 660u, 0x821bad58u, 0x821ea0d8u, 0x821ea310u},
    {0x82631be8u, 656u, 660u, 0x821bad58u, 0x821ea330u, 0x821ea568u},
    {0x826320a0u, 608u, 612u, 0x821bad58u, 0x821e9c40u, 0x821e9e88u},
    {0x82632250u, 608u, 612u, 0x821bad58u, 0x821eb740u, 0x821eb974u},
    {0x82632480u, 608u, 752u, 0x821bad58u, 0x821eb990u, 0x821ebbc0u},
    {0x82632648u, 608u, 748u, 0x821bad58u, 0x821ebbe0u, 0x821ebe10u},
    // END GENERATED INSTANCE UI PARAMETERS
};

} // namespace

const Spec* Find(GuestAddress address)
{
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& entry, GuestAddress target)
        { return entry.address < target; });
    return spec != std::end(kSpecs) && spec->address == address ? spec : nullptr;
}

bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, std::uint64_t& result)
{
    const Spec* spec = Find(address);
    if (!spec) return false;
    InitializeWith(*spec, memory, static_cast<GuestAddress>(incoming_r3));
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_ui_initializer_family
