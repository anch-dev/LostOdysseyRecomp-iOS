#include "lo_semantics/manager_init.h"

namespace lo::semantic::gpu
{
namespace
{
constexpr GuestAddress kManagerGlobal = 0x8330b608u;

[[nodiscard]] GuestAddress VtableMethod(GuestMemory& memory,
                                        std::uint64_t receiver_register,
                                        std::uint32_t slot_offset)
{
    const GuestAddress vtable = memory.ReadU32(
        static_cast<GuestAddress>(receiver_register));
    return memory.ReadU32(vtable + slot_offset) & ~GuestAddress{3};
}
} // namespace

std::uint64_t InitializeManager(GuestMemory& memory,
    ManagerInitServices& services, GuestAddress frame_base)
{
    std::uint64_t receiver = services.AllocateRaw(0x48decu);
    memory.WriteU32(frame_base + 80, static_cast<GuestAddress>(receiver));
    if (static_cast<GuestAddress>(receiver) != 0)
        receiver = services.ConstructPrimary(receiver);
    else
        receiver = 0;

    memory.WriteU32(kManagerGlobal, static_cast<GuestAddress>(receiver));
    const GuestAddress first_method = VtableMethod(memory, receiver, 60);
    const std::uint64_t first_result = services.CallMethod(first_method, receiver);

    if (static_cast<GuestAddress>(first_result) == 0)
    {
        receiver = services.AllocateRaw(36);
        memory.WriteU32(frame_base + 80, static_cast<GuestAddress>(receiver));
        if (static_cast<GuestAddress>(receiver) != 0)
        {
            const GuestAddress current_manager = memory.ReadU32(kManagerGlobal);
            receiver = services.ConstructFallback(receiver, current_manager);
        }
        else
            receiver = 0;
        memory.WriteU32(kManagerGlobal, static_cast<GuestAddress>(receiver));
    }
    else
        receiver = memory.ReadU32(kManagerGlobal);

    const GuestAddress second_method = VtableMethod(memory, receiver, 56);
    return services.CallMethod(second_method, receiver);
}

} // namespace lo::semantic::gpu
