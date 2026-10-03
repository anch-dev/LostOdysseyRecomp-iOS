#include "lo_semantics/metadata_member_dispatch.h"

namespace lo::semantic::gpu::metadata_member_dispatch
{
namespace
{
std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}
void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory, Services& services,
    std::uint64_t receiver, std::uint64_t type, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != 0x823fd400u) return false;
    const auto sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 112u, sp);
    frame.r31 = memory.ReadU32(static_cast<GuestAddress>(type + 120u));
    frame.r30 = receiver;
    result = receiver;
    while (static_cast<std::uint32_t>(frame.r31) != 0u)
    {
        const auto member = static_cast<GuestAddress>(frame.r31);
        if ((Read64(memory, member + 8u) & (1ull << 41u)) == 0u)
        {
            const GuestAddress table = memory.ReadU32(member);
            const std::uint32_t offset = memory.ReadU32(member + 100u);
            frame.ctr = memory.ReadU32(table + 340u);
            frame.lr = 0x823fd458u;
            result = services.CallMember(
                static_cast<GuestAddress>(frame.ctr) & ~3u,
                frame.r31, std::uint64_t{offset} + frame.r30,
                caller_sp - 112u, frame);
        }
        // The dynamic method can redirect live r31 or update its next link.
        frame.r31 = memory.ReadU32(static_cast<GuestAddress>(frame.r31 + 112u));
    }
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    return true;
}
} // namespace lo::semantic::gpu::metadata_member_dispatch
