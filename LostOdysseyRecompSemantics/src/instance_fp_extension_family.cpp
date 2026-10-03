#include "lo_semantics/instance_fp_extension_family.h"
#include "lo_semantics/loaded_single.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::instance_fp_extension_family
{
namespace
{

constexpr GuestAddress F13Source = 0x8218958cu;
constexpr GuestAddress F0Source = 0x82000e50u;

enum class Layout : std::uint8_t
{
    SkeletalMesh, LineBatch, Primitive, Model, Terrain, Brush, ForceFeedback,
};

struct Spec
{
    GuestAddress address;
    Layout layout;
    GuestAddress vtable;
    GuestAddress first_word;
    GuestAddress final_word;
};

// Each complete translated PPC body and its constants are checked by the
// generator before this small parameter table is admitted.
constexpr Spec kSpecs[] = {
    // BEGIN GENERATED INSTANCE FP EXTENSION PARAMETERS
    {0x82592690u, Layout::SkeletalMesh, 0x8200cf08u, 0u, 0u},
    {0x825aecf0u, Layout::LineBatch, 0x8200c5c0u, 0x821bad58u, 0x821da5d0u},
    {0x825e09b8u, Layout::Primitive, 0x8207354cu, 0u, 0u},
    {0x826bdc18u, Layout::Model, 0x82200ae8u, 0u, 0u},
    {0x826c00e8u, Layout::Terrain, 0x822013b0u, 0u, 0u},
    {0x8270dd80u, Layout::Brush, 0x8200b188u, 0u, 0u},
    {0x82730310u, Layout::ForceFeedback, 0x82003f88u, 0u, 0u},
    // END GENERATED INSTANCE FP EXTENSION PARAMETERS
};

LoadedSingle LoadSingle(GuestMemory& memory, GuestAddress address)
{
    return LoadedSingle::FromWord(memory.ReadU32(address));
}

void StoreSingle(GuestMemory& memory, GuestAddress address, LoadedSingle value)
{
    memory.WriteU32(address, value.StoreWord());
}

void ClearWords(GuestMemory& memory, GuestAddress object,
    GuestAddress first, GuestAddress last)
{
    for (GuestAddress offset = first; offset <= last; offset += 4u)
        memory.WriteU32(object + offset, 0u);
}

void WriteComponentPrefix(GuestMemory& memory, GuestAddress object,
    LoadedSingle f13)
{
    memory.WriteU32(object + 96u, 0u);
    ClearWords(memory, object, 368u, 376u);
    // The original std at +476 is one 64-bit access. These two word writes
    // model ordinary memory only, as in instance_component_initializer_family.
    memory.WriteU32(object + 476u, 0u);
    memory.WriteU32(object + 480u, 0u);
    StoreSingle(memory, object + 496u, f13);
}

void WriteComponentTail(GuestMemory& memory, GuestAddress object,
    LoadedSingle f0, LoadedSingle f13)
{
    for (GuestAddress offset : {504u, 508u, 512u})
        StoreSingle(memory, object + offset, f0);
    StoreSingle(memory, object + 516u, f13);
    for (GuestAddress offset : {520u, 524u, 528u, 532u})
        StoreSingle(memory, object + offset, f0);
    StoreSingle(memory, object + 536u, f13);
    for (GuestAddress offset : {540u, 544u, 548u, 552u})
        StoreSingle(memory, object + offset, f0);
    StoreSingle(memory, object + 556u, f13);
}

void Initialize(const Spec& spec, GuestMemory& memory,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    GuestAddress object, FpEffects& effects)
{
    if (spec.layout == Layout::ForceFeedback)
    {
        const auto flags = memory.ReadU32(object + 60u);
        fp_services.DisableFlushMode();
        const LoadedSingle f0 = LoadSingle(memory, F13Source);
        effects.f0 = f0.FprValue();
        StoreSingle(memory, object + 76u, f0);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 60u, flags | 0x80000000u);
        return;
    }

    fp_services.DisableFlushMode();
    const LoadedSingle f13 = LoadSingle(memory, F13Source);
    effects.f13 = f13.FprValue();

    // Primitive writes its vtable before loading the second single. This
    // matters when the object aliases that constant's guest address.
    if (spec.layout == Layout::Primitive)
        memory.WriteU32(object, spec.vtable);
    const LoadedSingle f0 = LoadSingle(memory, F0Source);
    effects.f0 = f0.FprValue();

    WriteComponentPrefix(memory, object, f13);
    if (spec.layout == Layout::Terrain)
        memory.WriteU32(object, spec.vtable);
    StoreSingle(memory, object + 500u, f0);
    if (spec.layout == Layout::SkeletalMesh || spec.layout == Layout::Model ||
        spec.layout == Layout::Brush)
        memory.WriteU32(object, spec.vtable);

    if (spec.layout == Layout::LineBatch)
    {
        memory.WriteU32(object + 624u, spec.first_word);
        StoreSingle(memory, object + 504u, f0);
        memory.WriteU32(object + 628u, 0u);
        for (GuestAddress offset : {508u, 512u})
            StoreSingle(memory, object + offset, f0);
        StoreSingle(memory, object + 516u, f13);
        for (GuestAddress offset : {520u, 524u, 528u, 532u})
            StoreSingle(memory, object + offset, f0);
        StoreSingle(memory, object + 536u, f13);
        for (GuestAddress offset : {540u, 544u, 548u, 552u})
            StoreSingle(memory, object + offset, f0);
        StoreSingle(memory, object + 556u, f13);
        memory.WriteU32(object, spec.vtable);
        memory.WriteU32(object + 624u, spec.final_word);
        ClearWords(memory, object, 632u, 640u);
        StoreSingle(memory, object + 644u, f0);
        return;
    }

    WriteComponentTail(memory, object, f0, f13);
    switch (spec.layout)
    {
    case Layout::SkeletalMesh:
        ClearWords(memory, object, 652u, 660u);
        ClearWords(memory, object, 688u, 696u);
        ClearWords(memory, object, 708u, 740u);
        ClearWords(memory, object, 948u, 980u);
        break;
    case Layout::Model:
        ClearWords(memory, object, 636u, 668u);
        break;
    case Layout::Terrain:
        ClearWords(memory, object, 624u, 644u);
        ClearWords(memory, object, 676u, 712u);
        ClearWords(memory, object, 740u, 772u);
        ClearWords(memory, object, 784u, 792u);
        break;
    case Layout::Brush:
        ClearWords(memory, object, 684u, 692u);
        break;
    case Layout::Primitive:
    case Layout::LineBatch:
    case Layout::ForceFeedback:
        break;
    }
}

} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    instance_component_initializer_family::ComponentFpServices& fp_services,
    std::uint64_t incoming_r3, FpEffects& effects, std::uint64_t& result)
{
    const Spec* spec = std::lower_bound(std::begin(kSpecs), std::end(kSpecs),
        address, [](const Spec& entry, GuestAddress target)
        { return entry.address < target; });
    if (spec == std::end(kSpecs) || spec->address != address)
        return false;
    const GuestAddress object = static_cast<GuestAddress>(incoming_r3);
    if (object != 0)
        Initialize(*spec, memory, fp_services, object, effects);
    result = incoming_r3;
    return true;
}

} // namespace lo::semantic::gpu::instance_fp_extension_family
