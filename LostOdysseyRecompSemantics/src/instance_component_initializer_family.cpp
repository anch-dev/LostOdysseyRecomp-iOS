#include "lo_semantics/instance_component_initializer_family.h"
#include "lo_semantics/loaded_single.h"

#include <algorithm>
#include <bit>
#include <initializer_list>
#include <iterator>

namespace lo::semantic::gpu::instance_component_initializer_family
{
namespace
{

struct Spec
{
    GuestAddress address;
    GuestAddress vtable;
};

// Each complete generated PPC body is checked before emitting its vtable.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE COMPONENT PARAMETERS
    {0x825c2c28u, 0x821db7c8u},
    {0x825e03d8u, 0x821ddbf0u},
    {0x825e0538u, 0x821dddd8u},
    {0x825e05b8u, 0x821dd630u},
    {0x825e0788u, 0x8200b370u},
    {0x825e0938u, 0x821d2c98u},
    {0x825f74e8u, 0x821e1488u},
    {0x826042b8u, 0x8200ee88u},
    {0x826b6478u, 0x82200168u},
    {0x826f8010u, 0x82207508u},
    {0x82715f70u, 0x8220c998u},
    {0x8271cc88u, 0x8220e338u},
    {0x827203c8u, 0x8220e7e8u},
    // END GENERATED INSTANCE COMPONENT PARAMETERS
};

LoadedSingle LoadSingle(GuestMemory& memory, GuestAddress address)
{
    return LoadedSingle::FromWord(memory.ReadU32(address));
}

void StoreSingle(GuestMemory& memory, GuestAddress address, LoadedSingle value)
{
    memory.WriteU32(address, value.StoreWord());
}

} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ComponentFpServices& fp_services, std::uint64_t incoming_r3,
    ComponentEffects& effects, std::uint64_t& result)
{
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& entry, GuestAddress target)
        { return entry.address < target; });
    if (spec == std::end(kSpecs) || spec->address != address)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    if (object != 0)
    {
        fp_services.DisableFlushMode();
        const LoadedSingle f13 = LoadSingle(memory, 0x8218958cu);
        const LoadedSingle f0 = LoadSingle(memory, 0x82000e50u);

        memory.WriteU32(object + 96u, 0u);
        memory.WriteU32(object + 368u, 0u);
        memory.WriteU32(object + 372u, 0u);
        memory.WriteU32(object + 376u, 0u);
        // GuestMemory models ordinary bounded memory with word writes. The
        // original PPC std at +476 is one 64-bit access for fault/MMIO purposes.
        memory.WriteU32(object + 476u, 0u);
        memory.WriteU32(object + 480u, 0u);
        StoreSingle(memory, object + 496u, f13);
        StoreSingle(memory, object + 500u, f0);
        memory.WriteU32(object, spec->vtable);
        for (GuestAddress offset : {504u, 508u, 512u})
            StoreSingle(memory, object + offset, f0);
        StoreSingle(memory, object + 516u, f13);
        for (GuestAddress offset : {520u, 524u, 528u, 532u})
            StoreSingle(memory, object + offset, f0);
        StoreSingle(memory, object + 536u, f13);
        for (GuestAddress offset : {540u, 544u, 548u, 552u})
            StoreSingle(memory, object + offset, f0);
        StoreSingle(memory, object + 556u, f13);

        effects.r10 = static_cast<std::uint64_t>(static_cast<std::int64_t>(
            std::bit_cast<std::int32_t>(spec->vtable)));
        effects.r11 = 0;
        effects.f0 = f0.FprValue();
        effects.f13 = f13.FprValue();
    }
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_component_initializer_family
