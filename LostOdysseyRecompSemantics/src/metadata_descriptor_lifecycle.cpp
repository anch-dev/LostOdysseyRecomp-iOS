#include "lo_semantics/metadata_descriptor_lifecycle.h"

#include <bit>

namespace lo::semantic::gpu::metadata_descriptor_lifecycle
{
namespace
{
constexpr GuestAddress kDispatch = 0x8240efc0u;
constexpr GuestAddress kRelease = 0x82401c58u;

void Write64(GuestMemory& memory, GuestAddress address, std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}
std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

void Enter(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame, bool save26)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (save26)
    {
        Write64(memory, sp - 56u, frame.r26);
        Write64(memory, sp - 48u, frame.r27);
        Write64(memory, sp - 40u, frame.r28);
    }
    Write64(memory, sp - 32u, frame.r29);
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - (save26 ? 144u : 112u), sp);
}
void Leave(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame, bool save26)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    if (save26)
    {
        frame.r26 = Read64(memory, sp - 56u);
        frame.r27 = Read64(memory, sp - 48u);
        frame.r28 = Read64(memory, sp - 40u);
    }
    frame.r29 = Read64(memory, sp - 32u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}

std::uint64_t ReleaseBuffer(GuestMemory& memory,
    ManagerFacadeServices& manager, std::uint64_t buffer,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    // The accepted facade exposes the algorithm, but its 823F3340 ABI
    // frame is also visible to this caller's owned save slots.
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 112u, sp);
    const std::uint64_t result = ReleaseManagerBuffer(memory, manager,
        buffer, sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    return result;
}

std::uint64_t ReleaseArray(GuestMemory& memory,
    ManagerFacadeServices& manager, std::uint64_t header,
    std::uint64_t caller_sp, FrameRegisters& frame)
{
    // 82507598 saves LR/r31 and enters a 96-byte frame before the accepted
    // RemoveArrayRange/ReleaseManagerBuffer composition.
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 96u, sp);
    const std::uint64_t result = ReleaseArrayElements(memory, manager,
        static_cast<GuestAddress>(header), 12u, 8u, sp);
    frame.lr = memory.ReadU32(sp - 8u);
    frame.r31 = Read64(memory, sp - 16u);
    return result;
}

void DispatchNodes(GuestMemory& memory, VirtualServices& methods,
    VolatileRegisters& registers, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, true);
    const std::uint64_t sp = caller_sp - 144u;
    frame.r31 = memory.ReadU32(static_cast<GuestAddress>(registers.r3) + 124u);
    frame.r30 = registers.r4;
    frame.r27 = registers.r5;
    frame.r26 = registers.r6;
    frame.r29 = registers.r7;
    frame.r28 = registers.r8;
    while (static_cast<GuestAddress>(frame.r31) != 0u)
    {
        const GuestAddress node = static_cast<GuestAddress>(frame.r31);
        if (static_cast<GuestAddress>(frame.r27) != 0u)
        {
            const std::uint32_t offset = memory.ReadU32(node + 100u);
            registers.r5 = std::uint64_t{offset} + frame.r27;
            if (std::bit_cast<std::int32_t>(offset) >=
                std::bit_cast<std::int32_t>(
                    static_cast<std::uint32_t>(frame.r26)))
                registers.r5 = 0u;
        }
        else
            registers.r5 = 0u;
        const GuestAddress vtable = memory.ReadU32(node);
        registers.r7 = frame.r28;
        const std::uint32_t live_offset = memory.ReadU32(node + 100u);
        registers.r6 = frame.r29;
        registers.r3 = frame.r31;
        registers.r4 = std::uint64_t{live_offset} + frame.r30;
        const GuestAddress method = memory.ReadU32(vtable + 360u);
        frame.ctr = method;
        frame.lr = 0x8240f02cu;
        methods.Dispatch(method & ~3u, memory, registers, sp, frame);
        frame.r31 = memory.ReadU32(
            static_cast<GuestAddress>(frame.r31) + 124u);
    }
    Leave(memory, caller_sp, frame, true);
}

void ReleaseObject(GuestMemory& memory, ManagerFacadeServices& manager,
    VolatileRegisters& registers, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    Enter(memory, caller_sp, frame, false);
    const std::uint64_t sp = caller_sp - 112u;
    frame.r31 = registers.r3;
    frame.r30 = frame.r31 + 52u;
    registers.r3 = memory.ReadU32(static_cast<GuestAddress>(frame.r30) + 12u);
    frame.lr = 0x82401c74u;
    registers.r3 = ReleaseBuffer(memory, manager, registers.r3, sp, frame);
    frame.r29 = 0u;
    registers.r3 = frame.r30;
    memory.WriteU32(static_cast<GuestAddress>(frame.r30) + 12u, 0u);
    memory.WriteU32(static_cast<GuestAddress>(frame.r30) + 16u, 0u);
    frame.lr = 0x82401c88u;
    registers.r3 = ReleaseArray(memory, manager, registers.r3, sp, frame);

    frame.r30 = frame.r31 + 32u;
    registers.r3 = memory.ReadU32(static_cast<GuestAddress>(frame.r30) + 12u);
    frame.lr = 0x82401c94u;
    registers.r3 = ReleaseBuffer(memory, manager, registers.r3, sp, frame);
    registers.r3 = frame.r30;
    memory.WriteU32(static_cast<GuestAddress>(frame.r30) + 12u, 0u);
    memory.WriteU32(static_cast<GuestAddress>(frame.r30) + 16u, 0u);
    frame.lr = 0x82401ca4u;
    registers.r3 = ReleaseArray(memory, manager, registers.r3, sp, frame);

    registers.r3 = frame.r31;
    frame.lr = 0x82401cacu;
    registers.r3 = ReleaseBuffer(memory, manager, registers.r3, sp, frame);
    registers.r3 = frame.r31;
    Leave(memory, caller_sp, frame, false);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerFacadeServices& manager, VirtualServices& methods,
    VolatileRegisters& registers, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    switch (address)
    {
    case kDispatch:
        DispatchNodes(memory, methods, registers, caller_sp, frame);
        result = registers.r3;
        return true;
    case kRelease:
        ReleaseObject(memory, manager, registers, caller_sp, frame);
        result = registers.r3;
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::metadata_descriptor_lifecycle
