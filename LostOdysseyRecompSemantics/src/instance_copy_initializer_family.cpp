#include "lo_semantics/instance_copy_initializer_family.h"
#include "lo_semantics/memory_move.h"

namespace lo::semantic::gpu::instance_copy_initializer_family
{
namespace
{
// BEGIN GENERATED INSTANCE COPY PARAMETERS
constexpr GuestAddress Entry = 0x8268cc28u;
constexpr GuestAddress Vtable = 0x821fa9c0u;
constexpr GuestAddress DefaultSource = 0x8323a520u;
// END GENERATED INSTANCE COPY PARAMETERS

void WriteU64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t ReadU64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32) |
        memory.ReadU32(address + 4u);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    std::uint64_t incoming_r3, const EntryAbi& abi, Result& result)
{
    if (address != Entry)
        return false;

    const auto sp = static_cast<GuestAddress>(abi.caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(abi.incoming_lr));
    WriteU64(memory, sp - 24u, abi.incoming_r30);
    WriteU64(memory, sp - 16u, abi.incoming_r31);
    memory.WriteU32(sp - 112u, sp);

    const auto object = static_cast<GuestAddress>(incoming_r3);
    std::uint64_t returned_r3 = incoming_r3;
    if (object != 0)
    {
        const auto flags = memory.ReadU32(object + 120u);
        memory.WriteU32(object, Vtable);
        memory.WriteU32(object + 120u, flags | 0x80000000u);
        (void)CopyGuestMemory(memory, incoming_r3 + 144u, DefaultSource,
            64u, sp - 112u);
        returned_r3 = CopyGuestMemory(memory, incoming_r3 + 208u,
            DefaultSource, 64u, sp - 112u);
    }

    result = {returned_r3, memory.ReadU32(sp - 8u),
        ReadU64(memory, sp - 24u), ReadU64(memory, sp - 16u)};
    return true;
}

} // namespace lo::semantic::gpu::instance_copy_initializer_family
