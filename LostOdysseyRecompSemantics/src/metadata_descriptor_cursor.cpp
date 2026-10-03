#include "lo_semantics/metadata_descriptor_cursor.h"

namespace lo::semantic::gpu::metadata_descriptor_cursor
{
namespace
{
constexpr GuestAddress kAdvance = 0x822a6ef8u;
constexpr GuestAddress kInitialize = 0x82406568u;

void Enter(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - 16u, static_cast<std::uint32_t>(frame.r31 >> 32u));
    memory.WriteU32(sp - 12u, static_cast<std::uint32_t>(frame.r31));
    memory.WriteU32(sp - 96u, sp);
}

void Leave(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r31 = (std::uint64_t{memory.ReadU32(sp - 16u)} << 32u) |
        memory.ReadU32(sp - 12u);
}

std::uint64_t Advance(GuestMemory& memory, VirtualServices& services,
    std::uint64_t header_register, std::uint64_t r4,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame);
    const std::uint64_t sp = caller_sp - 96u;
    frame.r31 = header_register;
    std::uint64_t result = header_register;
    if (memory.ReadU32(static_cast<GuestAddress>(frame.r31)) != 0u)
    {
        do
        {
            GuestAddress header = static_cast<GuestAddress>(frame.r31);
            if (memory.ReadU32(header + 4u) != 0u)
            {
                GuestAddress link = 0;
                do
                {
                    // The list head is reloaded on each visit; guest RAM may
                    // alias this cursor or have changed in a prior callback.
                    const GuestAddress current = memory.ReadU32(header + 4u);
                    const GuestAddress descriptor =
                        memory.ReadU32(current + 52u);
                    const std::uint32_t flags =
                        memory.ReadU32(descriptor + 184u);
                    if ((flags & 0x8000u) != 0u)
                    {
                        Leave(memory, caller_sp, frame);
                        return result;
                    }
                    link = memory.ReadU32(current + 64u);
                    memory.WriteU32(header + 4u, link);
                } while (link != 0u);
            }

            header = static_cast<GuestAddress>(frame.r31);
            const GuestAddress node = memory.ReadU32(header);
            const GuestAddress vtable = memory.ReadU32(node);
            const GuestAddress method = memory.ReadU32(vtable + 284u);
            frame.ctr = method;
            frame.lr = 0x822a6f64u;
            result = services.CallNext(method & ~3u, memory, node,
                r4, sp, frame);

            header = static_cast<GuestAddress>(frame.r31);
            const GuestAddress returned = static_cast<GuestAddress>(result);
            memory.WriteU32(header, returned);
            if (returned != 0u)
                memory.WriteU32(header + 4u,
                    memory.ReadU32(returned + 76u));
        } while (memory.ReadU32(
            static_cast<GuestAddress>(frame.r31)) != 0u);
    }
    Leave(memory, caller_sp, frame);
    return result;
}

std::uint64_t Initialize(GuestMemory& memory, VirtualServices& services,
    std::uint64_t header_register, std::uint64_t node_register,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame);
    const std::uint64_t sp = caller_sp - 96u;
    frame.r31 = header_register;
    const GuestAddress header = static_cast<GuestAddress>(frame.r31);
    const GuestAddress node = static_cast<GuestAddress>(node_register);
    memory.WriteU32(header, node);
    const std::uint32_t link = node == 0u ?
        0u : memory.ReadU32(node + 76u);
    memory.WriteU32(header + 4u, link);
    frame.lr = 0x824065a0u;
    (void)Advance(memory, services, frame.r31, node_register, sp, frame);
    const std::uint64_t result = frame.r31;
    Leave(memory, caller_sp, frame);
    return result;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    VirtualServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case kAdvance:
        result = Advance(memory, services, incoming_r3, incoming_r4,
            caller_sp, frame);
        return true;
    case kInitialize:
        result = Initialize(memory, services, incoming_r3, incoming_r4,
            caller_sp, frame);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::metadata_descriptor_cursor
