#include "lo_semantics/registered_metadata_initializers.h"

#include <bit>
#include <initializer_list>

namespace lo::semantic::gpu::registered_metadata_initializers
{
namespace
{
double LoadSingle(GuestMemory& memory, GuestAddress address)
{
    return static_cast<double>(std::bit_cast<float>(memory.ReadU32(address)));
}

void StoreSingle(GuestMemory& memory, GuestAddress address, double value)
{
    memory.WriteU32(address, std::bit_cast<std::uint32_t>(
        static_cast<float>(value)));
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t object_register, Effects& effects)
{
    if (address != 0x825A8448u && address != 0x826DA6B0u &&
        address != 0x8270BBE8u)
        return false;

    const GuestAddress object = static_cast<GuestAddress>(object_register);
    effects.disable_flush_mode = true;
    switch (address)
    {
    case 0x825A8448u:
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2113929216});
        effects.r10 = memory.ReadU32(object + 60u) | 0x80000000u;
        effects.f0 = LoadSingle(memory, 0x82000E50u);
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2112290816});
        StoreSingle(memory, object + 76u, effects.f0);
        StoreSingle(memory, object + 72u, effects.f0);
        memory.WriteU32(object + 60u, static_cast<std::uint32_t>(effects.r10));
        StoreSingle(memory, object + 68u, effects.f0);
        StoreSingle(memory, object + 64u, effects.f0);
        effects.f13 = LoadSingle(memory, 0x8218958Cu);
        for (GuestAddress offset : {92u, 88u, 84u, 80u})
            StoreSingle(memory, object + offset, effects.f13);
        break;

    case 0x826DA6B0u:
        effects.r10 = static_cast<std::uint64_t>(std::int64_t{-2112290816});
        effects.r11 = 1;
        effects.f0 = LoadSingle(memory, 0x8218958Cu);
        effects.r10 = static_cast<std::uint64_t>(std::int64_t{-2113929216});
        StoreSingle(memory, object + 84u, effects.f0);
        memory.WriteU32(object + 208u, 1);
        memory.WriteU32(object + 212u, 1);
        memory.WriteU32(object + 220u, 1);
        effects.f13 = LoadSingle(memory, 0x82000C84u);
        effects.r10 = 0;
        StoreSingle(memory, object + 88u, effects.f13);
        memory.WriteU32(object + 216u, 0);
        break;

    case 0x8270BBE8u:
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2112290816});
        effects.r10 = 1;
        memory.WriteU32(object + 80u, 1);
        effects.f0 = LoadSingle(memory, 0x821894F8u);
        effects.f13 = LoadSingle(memory, 0x82189400u);
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2094858240});
        StoreSingle(memory, object + 72u, effects.f0);
        StoreSingle(memory, object + 76u, effects.f13);
        effects.r11 = memory.ReadU32(0x83235AE4u);
        memory.WriteU32(object + 84u, static_cast<std::uint32_t>(effects.r11));
        effects.r11 = static_cast<std::uint64_t>(std::int64_t{-2094858240});
        effects.r11 = memory.ReadU32(0x83235AE8u);
        memory.WriteU32(object + 88u, static_cast<std::uint32_t>(effects.r11));
        break;
    }
    return true;
}

} // namespace lo::semantic::gpu::registered_metadata_initializers
