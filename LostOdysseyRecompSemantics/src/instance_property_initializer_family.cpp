#include "lo_semantics/instance_property_initializer_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_property_initializer_family
{
namespace
{

// All five stores and their order are identical across the family. The
// generator validates the entire PPC body before emitting each vtable word.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE PROPERTY PARAMETERS
    {0x825f1018u, 0x82008250u},
    {0x825f1100u, 0x821dfb28u},
    {0x825f11e8u, 0x82008e00u},
    {0x825f1218u, 0x820086d0u},
    {0x825f1248u, 0x820083d0u},
    {0x825f1278u, 0x821dfe28u},
    {0x825f1360u, 0x821df9a8u},
    {0x825f1410u, 0x82008f80u},
    {0x825f1440u, 0x82008b00u},
    {0x825f1470u, 0x82008980u},
    {0x825f14a0u, 0x821df828u},
    {0x825f1588u, 0x821dfca8u},
    {0x825f1670u, 0x82008550u},
    {0x825f16a0u, 0x82008c80u},
    {0x825f1788u, 0x821a7d70u},
    // END GENERATED INSTANCE PROPERTY PARAMETERS
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

} // namespace lo::semantic::gpu::instance_property_initializer_family
