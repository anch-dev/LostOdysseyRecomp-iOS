#include "lo_semantics/array_release_family.h"

namespace lo::semantic::gpu::array_release_family
{
namespace
{
struct Parameters
{
    GuestAddress address;
    std::uint32_t element_size;
    std::uint32_t resize_argument;
};
constexpr Parameters kEntries[] = {
    {0x823173E8u, 64, 8},
    {0x82405E08u, 1, 8},
    {0x8245C2F0u, 84, 8},
    {0x82478CA0u, 52, 8},
    {0x824E2A38u, 48, 8},
    {0x82507598u, 12, 8},
    {0x825075F8u, 8, 8},
    {0x82537390u, 4, 8},
    {0x8255C7B0u, 2, 4},
    {0x8255C8A8u, 40, 4},
    {0x8255DE28u, 68, 8},
    {0x8256B518u, 372, 8},
    {0x825920D0u, 56, 8},
    {0x82679C18u, 60, 8},
    {0x82695A00u, 36, 8},
    {0x826A9AF0u, 152, 8},
    {0x826DDEE0u, 48, 4},
    {0x826DE038u, 56, 4},
    {0x826DE190u, 64, 4},
    {0x826DE3D0u, 16, 4},
    {0x826DE5F8u, 4, 4},
    {0x827023A8u, 40, 8},
    {0x82702408u, 16, 8},
    {0x82703C70u, 24, 8},
    {0x82703CD0u, 28, 8},
    {0x82703D30u, 20, 8},
    {0x82703D90u, 32, 8},
    {0x82717D78u, 96, 8},
    {0x82788420u, 44, 8},
    {0x8278CD28u, 80, 8},
    {0x827946D0u, 256, 8},
};
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& services, GuestAddress array,
    GuestAddress caller_sp, std::uint64_t& result)
{
    for (const Parameters& entry : kEntries)
    {
        if (entry.address != address) continue;
        result = ReleaseArrayElements(memory, services, array,
            entry.element_size, entry.resize_argument, caller_sp);
        return true;
    }
    return false;
}
} // namespace lo::semantic::gpu::array_release_family
