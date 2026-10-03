#include "lo_semantics/metadata_manager_dispatch.h"

namespace lo::semantic::gpu::metadata_manager_dispatch
{
namespace
{
constexpr GuestAddress kEntry = 0x823f3298u;
constexpr GuestAddress kManagerGlobal = 0x8330b608u;

void Write64(GuestMemory& memory, GuestAddress address,
    std::uint64_t value)
{
    memory.WriteU32(address, static_cast<std::uint32_t>(value >> 32u));
    memory.WriteU32(address + 4u, static_cast<std::uint32_t>(value));
}

std::uint64_t Read64(GuestMemory& memory, GuestAddress address)
{
    return (std::uint64_t{memory.ReadU32(address)} << 32u) |
        memory.ReadU32(address + 4u);
}

void SaveFrame(GuestMemory& memory, std::uint64_t caller_sp,
    const FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    Write64(memory, sp - 32u, frame.r29);
    Write64(memory, sp - 24u, frame.r30);
    Write64(memory, sp - 16u, frame.r31);
    memory.WriteU32(sp - 8u, static_cast<std::uint32_t>(frame.lr));
    memory.WriteU32(sp - 112u, sp);
}

void RestoreFrame(GuestMemory& memory, std::uint64_t caller_sp,
    FrameRegisters& frame)
{
    const GuestAddress sp = static_cast<GuestAddress>(caller_sp);
    frame.r29 = Read64(memory, sp - 32u);
    frame.r30 = Read64(memory, sp - 24u);
    frame.r31 = Read64(memory, sp - 16u);
    frame.lr = memory.ReadU32(sp - 8u);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    ManagerDispatchServices& services, std::uint64_t incoming_r3,
    std::uint64_t incoming_r4, std::uint64_t caller_sp,
    FrameRegisters& frame, std::uint64_t& result)
{
    if (address != kEntry)
        return false;

    SaveFrame(memory, caller_sp, frame);
    const std::uint64_t sp = caller_sp - 112u;
    frame.r31 = 0xffffffff83310000ull;
    frame.r30 = incoming_r3;
    frame.r29 = incoming_r4;
    GuestAddress manager = memory.ReadU32(kManagerGlobal);
    if (manager == 0)
    {
        frame.lr = 0x823f32c0u;
        (void)InitializeManager(memory, services,
            static_cast<GuestAddress>(sp - 112u));
        manager = memory.ReadU32(kManagerGlobal);
    }

    const GuestAddress vtable = memory.ReadU32(manager);
    const GuestAddress method = memory.ReadU32(vtable + 4u);
    frame.ctr = method;
    frame.lr = 0x823f32dcu;
    result = services.CallDescriptorMethod(method & ~3u, memory, manager,
        frame.r30, frame.r29, sp, frame.lr, frame);
    RestoreFrame(memory, caller_sp, frame);
    return true;
}

} // namespace lo::semantic::gpu::metadata_manager_dispatch
