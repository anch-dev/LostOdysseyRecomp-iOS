#include "lo_semantics/instance_field_initializer_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_field_initializer_family
{
namespace
{

// Every source body is checked in full before its parameters are emitted.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE FIELD INITIALIZER PARAMETERS
    {0x82570038u, 60u, 0x82065e9cu, 0x82065ee0u, 0x8200df68u, 0x8202bf78u, 0x82030dc0u},
    {0x82570170u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821cfdc8u, 0x8202bf78u, 0x82030dc0u},
    {0x825702a8u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821cff40u, 0x8202bf78u, 0x82030dc0u},
    {0x825703e0u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d00b8u, 0x821cfaf8u, 0x82030dc0u},
    {0x82570520u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d0230u, 0x821cfaf8u, 0x821d03a8u},
    {0x82570700u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821cf808u, 0x821cfaf8u, 0x82030dc0u},
    {0x825709d0u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d0838u, 0x821cfaf8u, 0x82030dc0u},
    {0x82570ac0u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821cf980u, 0x821cfaf8u, 0x82030dc0u},
    {0x82570cf0u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821cf078u, 0x821cf208u, 0x82030dc0u},
    {0x82570e28u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d09b0u, 0x8202d440u, 0x82030dc0u},
    {0x82570f58u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d0b28u, 0x8202d440u, 0x82030dc0u},
    {0x82571048u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821cfb38u, 0x8202d440u, 0x82030dc0u},
    {0x82571140u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d0ca0u, 0x8202d440u, 0x82030dc0u},
    {0x82571230u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d0e18u, 0x8202d440u, 0x82030dc0u},
    {0x82571320u, 60u, 0x82065e9cu, 0x82065ee0u, 0x821d0f90u, 0x8202d440u, 0x82030dc0u},
    {0x82582a90u, 60u, 0x82065e9cu, 0x82065ee0u, 0x820661a4u, 0x8206615cu, 0x82066130u},
    {0x82656ca0u, 88u, 0x821ee320u, 0x821590c4u, 0x821eed00u, 0x821ef0a0u, 0x821ef0bcu},
    {0x82656d98u, 88u, 0x821ee320u, 0x821590c4u, 0x821eef70u, 0x821ef0a0u, 0x821ef0bcu},
    {0x826a0c88u, 108u, 0x821590c4u, 0x821fd258u, 0x821fdda0u, 0x821fdeb8u, 0x821fdec8u},
    // END GENERATED INSTANCE FIELD INITIALIZER PARAMETERS
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

} // namespace lo::semantic::gpu::instance_field_initializer_family
