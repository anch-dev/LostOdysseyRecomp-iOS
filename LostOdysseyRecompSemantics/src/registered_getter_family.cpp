#include "lo_semantics/registered_getter_family.h"
#include "lo_semantics/registered_inline_constructor.h"

#include <algorithm>
#include <iterator>

namespace lo::semantic::gpu::registered_getter_family
{
namespace
{
struct Singleton
{
    GuestAddress address;
    GuestAddress global;
    GuestAddress constructor;
    GuestAddress registration;
    std::uint64_t owner;
};

constexpr Singleton kSingletons[] = {
    // BEGIN GENERATED REGISTERED GETTER PARAMETERS
    {0x822A1DD8u, 0x8331811Cu, 0x824C5CF0u, 0x824C5DA8u, 0xFFFFFFFF8218C21Cull},
    {0x822A2500u, 0x833184A0u, 0x82537698u, 0x82537750u, 0xFFFFFFFF8218C21Cull},
    {0x822A2700u, 0x833181E4u, 0x824C94B8u, 0x824C9570u, 0xFFFFFFFF8218C21Cull},
    {0x822A8828u, 0x83318760u, 0x825BABE8u, 0x825BACA0u, 0xFFFFFFFF8218C21Cull},
    {0x822A8978u, 0x833184C8u, 0x8255EF80u, 0x8255F038u, 0xFFFFFFFF8218C21Cull},
    {0x822B1FD0u, 0x83318528u, 0x8256C500u, 0x8256C5B8u, 0xFFFFFFFF8218C21Cull},
    {0x822C0F50u, 0x83318788u, 0x825DB1F0u, 0x825DB2A8u, 0xFFFFFFFF8218C21Cull},
    {0x822C9910u, 0x83318E54u, 0x826D82C8u, 0x826D8380u, 0xFFFFFFFF8218C21Cull},
    {0x822F9658u, 0x83318CB8u, 0x826816B0u, 0x82681768u, 0xFFFFFFFF8218C21Cull},
    {0x823065C0u, 0x83318CCCu, 0x82681DD8u, 0x82681E90u, 0xFFFFFFFF8218C21Cull},
    {0x8236C358u, 0x83318474u, 0x825249C0u, 0x82524A78u, 0xFFFFFFFF8218C21Cull},
    {0x824069E8u, 0x83315F80u, 0x82409540u, 0x824095F8u, 0xFFFFFFFF8218C210ull},
    {0x82406CC0u, 0x83315F60u, 0x82403148u, 0x82403200u, 0xFFFFFFFF8218C210ull},
    {0x8240C090u, 0x83318830u, 0x825F6A60u, 0x825F6B18u, 0xFFFFFFFF8218C210ull},
    {0x8241C328u, 0x833186C8u, 0x8259F000u, 0x8259F0B8u, 0xFFFFFFFF8218C21Cull},
    {0x82422370u, 0x833182C0u, 0x824EE758u, 0x824EE810u, 0xFFFFFFFF8218C21Cull},
    {0x824223C0u, 0x833182C4u, 0x824EE8E8u, 0x824EE9A0u, 0xFFFFFFFF8218C21Cull},
    {0x82422E78u, 0x833183D8u, 0x824F3118u, 0x824F31D0u, 0xFFFFFFFF8218C21Cull},
    {0x824246E8u, 0x833182E0u, 0x824EF190u, 0x824EF248u, 0xFFFFFFFF8218C21Cull},
    {0x82425FF8u, 0x833182C8u, 0x824EEAB8u, 0x824EEB70u, 0xFFFFFFFF8218C21Cull},
    {0x82426888u, 0x833182CCu, 0x824EEC48u, 0x824EED00u, 0xFFFFFFFF8218C21Cull},
    {0x82428218u, 0x83318558u, 0x825726A0u, 0x82572758u, 0xFFFFFFFF8218C21Cull},
    {0x824283E8u, 0x83318574u, 0x82573180u, 0x82573238u, 0xFFFFFFFF8218C21Cull},
    {0x82428788u, 0x83318578u, 0x825733A8u, 0x82573460u, 0xFFFFFFFF8218C21Cull},
    {0x82428B98u, 0x83318568u, 0x82572D20u, 0x82572DD8u, 0xFFFFFFFF8218C21Cull},
    {0x8242B638u, 0x833188D0u, 0x8260C2C0u, 0x8260C378u, 0xFFFFFFFF8218C21Cull},
    {0x8242C048u, 0x83318934u, 0x82612220u, 0x826122D8u, 0xFFFFFFFF8218C21Cull},
    {0x8242C9E8u, 0x833188F8u, 0x82610D58u, 0x82610E10u, 0xFFFFFFFF8218C21Cull},
    {0x8242D0F8u, 0x83318904u, 0x82611100u, 0x826111B8u, 0xFFFFFFFF8218C21Cull},
    {0x8242D448u, 0x83318948u, 0x82614E90u, 0x82614F48u, 0xFFFFFFFF8218C21Cull},
    {0x8242D918u, 0x83318908u, 0x826112B8u, 0x82611370u, 0xFFFFFFFF8218C21Cull},
    {0x8242DAE8u, 0x8331890Cu, 0x82611470u, 0x82611528u, 0xFFFFFFFF8218C21Cull},
    {0x8242DD08u, 0x83318910u, 0x82611628u, 0x826116E0u, 0xFFFFFFFF8218C21Cull},
    {0x8242DF28u, 0x83318914u, 0x826117E0u, 0x82611898u, 0xFFFFFFFF8218C21Cull},
    {0x8242E3F8u, 0x8331891Cu, 0x82611A90u, 0x82611B48u, 0xFFFFFFFF8218C21Cull},
    {0x8242E688u, 0x83318EA0u, 0x826F5618u, 0x826F56D0u, 0xFFFFFFFF8218C21Cull},
    {0x8242E918u, 0x83318920u, 0x82611C48u, 0x82611D00u, 0xFFFFFFFF8218C21Cull},
    {0x8242FDD0u, 0x833184C4u, 0x8255EDD0u, 0x8255EE88u, 0xFFFFFFFF8218C21Cull},
    {0x82432DC0u, 0x83318524u, 0x8256C350u, 0x8256C408u, 0xFFFFFFFF8218C21Cull},
    {0x8245D728u, 0x83318200u, 0x824C9CA8u, 0x824C9D60u, 0xFFFFFFFF8218C21Cull},
    {0x8245DD10u, 0x8331871Cu, 0x825A8178u, 0x825A8230u, 0xFFFFFFFF8218C21Cull},
    {0x8245DD60u, 0x8331809Cu, 0x8248BDE0u, 0x8248BE98u, 0xFFFFFFFF8218C21Cull},
    {0x8245E230u, 0x83318790u, 0x825DB560u, 0x825DB618u, 0xFFFFFFFF8218C21Cull},
    {0x8245F818u, 0x8331877Cu, 0x825CC9F0u, 0x825CCAA8u, 0xFFFFFFFF8218C21Cull},
    {0x8245FB68u, 0x83318FDCu, 0x827262A8u, 0x82726360u, 0xFFFFFFFF8218C21Cull},
    {0x8245FF08u, 0x83318204u, 0x824C9E78u, 0x824C9F30u, 0xFFFFFFFF8218C21Cull},
    {0x82460368u, 0x83318194u, 0x824C7C70u, 0x824C7D28u, 0xFFFFFFFF8218C21Cull},
    {0x824605F8u, 0x8331878Cu, 0x825DB3A8u, 0x825DB460u, 0xFFFFFFFF8218C21Cull},
    {0x82460818u, 0x8331861Cu, 0x8259C2F0u, 0x8259C3A8u, 0xFFFFFFFF8218C21Cull},
    {0x82460868u, 0x83318718u, 0x825A50E0u, 0x825A5198u, 0xFFFFFFFF8218C21Cull},
    {0x82460CC8u, 0x83318114u, 0x824C37F8u, 0x824C38B0u, 0xFFFFFFFF8218C21Cull},
    {0x82460D18u, 0x83318120u, 0x824C5EA0u, 0x824C5F58u, 0xFFFFFFFF8218C21Cull},
    {0x82460FA8u, 0x83318730u, 0x825AA7F8u, 0x825AA8B8u, 0xFFFFFFFF8218C21Cull},
    {0x824614C8u, 0x83318564u, 0x82572B70u, 0x82572C28u, 0xFFFFFFFF8218C21Cull},
    {0x82462980u, 0x833181DCu, 0x824C90D8u, 0x824C9190u, 0xFFFFFFFF8218C21Cull},
    {0x82463F40u, 0x83318C6Cu, 0x8267D950u, 0x8267DA08u, 0xFFFFFFFF8218C21Cull},
    {0x82463F90u, 0x833180F4u, 0x824C1728u, 0x824C17E0u, 0xFFFFFFFF8218C21Cull},
    {0x82463FE0u, 0x83247210u, 0x827CE240u, 0x827CE088u, 0xFFFFFFFF8218C21Cull},
    // END GENERATED REGISTERED GETTER PARAMETERS
};
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    std::uint64_t /* incoming_r3 */, GuestAddress caller_sp, std::uint64_t& result)
{
    const Singleton* entry = std::lower_bound(std::begin(kSingletons),
        std::end(kSingletons), address,
        [](const Singleton& spec, GuestAddress target)
        { return spec.address < target; });
    if (entry == std::end(kSingletons) || entry->address != address)
        return false;

    GuestAddress object = memory.ReadU32(entry->global);
    if (object == 0)
    {
        const GuestAddress frame = caller_sp - 96u;
        std::uint64_t constructed = 0;
        if (entry->constructor == 0x827ce240u)
            constructed = ConstructInlineManagedRegisteredObject(memory,
                manager_services, entry->owner, frame);
        else
        {
            // Other targets are recovered ordinary constructors; the strict
            // generator rejects unknown and lazy constructor variants.
            (void)registered_constructor_family::Apply(entry->constructor, memory,
                manager_services, registration_services, entry->owner, frame,
                constructed);
        }
        memory.WriteU32(entry->global, static_cast<GuestAddress>(constructed));
        (void)registration_services.Register(entry->registration, constructed,
            frame);
        object = memory.ReadU32(entry->global);
    }
    result = object;
    return true;
}

} // namespace lo::semantic::gpu::registered_getter_family
