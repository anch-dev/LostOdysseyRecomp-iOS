#include "lo_semantics/owned_state_initializer.h"

#include <initializer_list>

namespace lo::semantic::gpu::owned_state_initializer
{
namespace
{
void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    const std::uint64_t high = memory.ReadU32(address);
    return (high << 32u) | memory.ReadU32(address + 4u);
}

std::uint64_t InitializeState(GuestMemory& memory, std::uint64_t state,
    std::uint64_t owner)
{
    const GuestAddress object = static_cast<GuestAddress>(state);
    const GuestAddress source = static_cast<GuestAddress>(owner);
    memory.WriteU32(object, 0x82189958u);
    const std::uint32_t first_owner_word = source == 0 ? 0u :
        memory.ReadU32(source + 52u);
    memory.WriteU32(object + 4u, first_owner_word);
    memory.WriteU32(object + 8u, source);
    for (GuestAddress offset : {12u, 16u, 20u, 24u})
        memory.WriteU32(object + offset, 0);
    memory.WriteU32(object, 0x8218D8ACu);
    const std::uint32_t live_owner_word = memory.ReadU32(source + 52u);
    WriteU64(memory, object + 32u, UINT64_MAX);
    memory.WriteU32(object + 28u, live_owner_word);
    for (GuestAddress offset : {44u, 48u, 52u})
        memory.WriteU32(object + offset, 0);
    return state;
}

std::uint64_t ReplaceState(GuestMemory& memory,
    ManagerFacadeServices& manager_services, StateServices& state_services,
    std::uint64_t owner, std::uint64_t caller_sp, FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    WriteU64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 96u, sp);
    frame.r31 = owner;
    const std::uint64_t old_state = memory.ReadU32(
        static_cast<GuestAddress>(frame.r31) + 24u);
    if (old_state != 0)
    {
        const GuestAddress vtable = memory.ReadU32(
            static_cast<GuestAddress>(old_state));
        const GuestAddress method = memory.ReadU32(vtable) & ~3u;
        frame.lr = 0x823FA03Cu;
        state_services.DestroyState(method, memory, old_state, 1,
            caller_sp - 96u, frame);
    }
    frame.lr = 0x823FA044u;
    std::uint64_t result = AllocateManagerBuffer(memory, manager_services,
        56, sp - 96u);
    if (static_cast<GuestAddress>(result) != 0)
    {
        frame.lr = 0x823FA054u;
        result = InitializeState(memory, result, frame.r31);
    }
    else
        result = 0;

    const GuestAddress live_owner = static_cast<GuestAddress>(frame.r31);
    const std::uint64_t flags = ReadU64(memory, live_owner + 8u);
    memory.WriteU32(live_owner + 24u, static_cast<GuestAddress>(result));
    WriteU64(memory, live_owner + 8u, flags | (std::uint64_t{1} << 57u));
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r31 = ReadU64(memory, sp - 16u);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager_services, StateServices& state_services,
    std::uint64_t incoming_r3, std::uint64_t incoming_r4,
    std::uint64_t caller_sp, FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case 0x82406A38u:
        result = InitializeState(memory, incoming_r3, incoming_r4);
        return true;
    case 0x823FA008u:
        result = ReplaceState(memory, manager_services, state_services,
            incoming_r3, caller_sp, frame);
        return true;
    default:
        return false;
    }
}

} // namespace lo::semantic::gpu::owned_state_initializer
