#include "lo_semantics/registered_metadata_initializers.h"
#include "lo_semantics/loaded_single.h"

#include <initializer_list>

namespace lo::semantic::gpu::registered_metadata_initializers
{
namespace
{
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
    FpServices& fp_services, std::uint64_t object_register, Effects& effects)
{
    if (address != 0x825A8448u && address != 0x826DA6B0u &&
        address != 0x8270BBE8u)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(object_register);
    switch (address)
    {
    case 0x825A8448u:
    {
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2113929216});
        effects.r10 = memory.ReadU32(object + 60u) | 0x80000000u;
        fp_services.DisableFlushMode();
        effects.disable_flush_mode = true;
        const auto f0 = LoadSingle(memory, 0x82000E50u);
        effects.f0 = f0.FprValue();
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2112290816});
        StoreSingle(memory, object + 76u, f0);
        StoreSingle(memory, object + 72u, f0);
        memory.WriteU32(object + 60u, static_cast<std::uint32_t>(effects.r10));
        StoreSingle(memory, object + 68u, f0);
        StoreSingle(memory, object + 64u, f0);
        const auto f13 = LoadSingle(memory, 0x8218958Cu);
        effects.f13 = f13.FprValue();
        for (GuestAddress offset : {92u, 88u, 84u, 80u})
            StoreSingle(memory, object + offset, f13);
        break;
    }

    case 0x826DA6B0u:
    {
        effects.r10 = static_cast<std::uint64_t>(std::int64_t{-2112290816});
        effects.r11 = 1;
        fp_services.DisableFlushMode();
        effects.disable_flush_mode = true;
        const auto f0 = LoadSingle(memory, 0x8218958Cu);
        effects.f0 = f0.FprValue();
        effects.r10 = static_cast<std::uint64_t>(std::int64_t{-2113929216});
        StoreSingle(memory, object + 84u, f0);
        memory.WriteU32(object + 208u, 1);
        memory.WriteU32(object + 212u, 1);
        memory.WriteU32(object + 220u, 1);
        const auto f13 = LoadSingle(memory, 0x82000C84u);
        effects.f13 = f13.FprValue();
        effects.r10 = 0;
        StoreSingle(memory, object + 88u, f13);
        memory.WriteU32(object + 216u, 0);
        break;
    }

    case 0x8270BBE8u:
    {
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2112290816});
        effects.r10 = 1;
        memory.WriteU32(object + 80u, 1);
        fp_services.DisableFlushMode();
        effects.disable_flush_mode = true;
        const auto f0 = LoadSingle(memory, 0x821894F8u);
        effects.f0 = f0.FprValue();
        const auto f13 = LoadSingle(memory, 0x82189400u);
        effects.f13 = f13.FprValue();
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2094858240});
        StoreSingle(memory, object + 72u, f0);
        StoreSingle(memory, object + 76u, f13);
        effects.r11 = memory.ReadU32(0x83235AE4u);
        memory.WriteU32(object + 84u, static_cast<std::uint32_t>(effects.r11));
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2094858240});
        effects.r11 = memory.ReadU32(0x83235AE8u);
        memory.WriteU32(object + 88u, static_cast<std::uint32_t>(effects.r11));
        break;
    }
    }
    return true;
}

} // namespace lo::semantic::gpu::registered_metadata_initializers
