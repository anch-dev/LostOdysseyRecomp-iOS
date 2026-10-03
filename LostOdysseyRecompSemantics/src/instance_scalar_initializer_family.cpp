#include "lo_semantics/instance_scalar_initializer_family.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_scalar_initializer_family
{
namespace
{
enum class Layout : std::uint8_t { ClearBeforeVtable, FourZerosAndEight, ThreeZeros };

struct Spec
{
    GuestAddress address;
    GuestAddress vtable;
    std::uint16_t first_field;
    Layout layout;
};

// Each row is admitted only after matching its full cached PPC body and CFG.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE SCALAR PARAMETERS
    {0x8240ab00u, 0x82190d70u, 68u, Layout::ThreeZeros},
    {0x8240ac40u, 0x821910b8u, 60u, Layout::FourZerosAndEight},
    {0x82411f48u, 0x821914b0u, 68u, Layout::ThreeZeros},
    {0x82483b20u, 0x821a8b50u, 60u, Layout::ThreeZeros},
    {0x824d5c80u, 0x821b3790u, 72u, Layout::ClearBeforeVtable},
    {0x824d8bf8u, 0x821b32f0u, 72u, Layout::ClearBeforeVtable},
    {0x824d8cd0u, 0x821b31c8u, 72u, Layout::ClearBeforeVtable},
    {0x824d8da8u, 0x821b3668u, 72u, Layout::ClearBeforeVtable},
    {0x824d8e68u, 0x821b3540u, 72u, Layout::ClearBeforeVtable},
    {0x824d8f30u, 0x821b2c00u, 72u, Layout::ClearBeforeVtable},
    {0x824d8ff0u, 0x821b2e50u, 72u, Layout::ClearBeforeVtable},
    {0x8256f938u, 0x82007ee8u, 232u, Layout::FourZerosAndEight},
    {0x82599f30u, 0x821d31e8u, 84u, Layout::FourZerosAndEight},
    {0x82603c78u, 0x821e2f38u, 72u, Layout::FourZerosAndEight},
    {0x82656288u, 0x821ee5a0u, 100u, Layout::ThreeZeros},
    {0x8266eb08u, 0x821f24a0u, 256u, Layout::FourZerosAndEight},
    {0x8268cdc8u, 0x821fb168u, 100u, Layout::FourZerosAndEight},
    {0x826e2298u, 0x82203e50u, 92u, Layout::FourZerosAndEight},
    {0x82720e78u, 0x8220ecb8u, 72u, Layout::ThreeZeros},
    // END GENERATED INSTANCE SCALAR PARAMETERS
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
        if (spec->layout == Layout::ClearBeforeVtable)
        {
            memory.WriteU32(object + spec->first_field, 0);
            memory.WriteU32(object, spec->vtable);
        }
        else
        {
            memory.WriteU32(object, spec->vtable);
            const unsigned zeros = spec->layout == Layout::FourZerosAndEight ? 4u : 3u;
            for (unsigned index = 0; index < zeros; ++index)
                memory.WriteU32(object + spec->first_field + index * 4u, 0);
            if (spec->layout == Layout::FourZerosAndEight)
                memory.WriteU32(object + spec->first_field + 16u, 8);
        }
    }
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_scalar_initializer_family
