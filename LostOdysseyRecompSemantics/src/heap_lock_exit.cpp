#include "lo_semantics/heap_lock_exit.h"

namespace lo::semantic::gpu::heap_lock_exit
{
namespace
{
GuestAddress Address(std::uint64_t full)
{
    return static_cast<GuestAddress>(full);
}

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
    NativeServices& native, Registers& registers)
{
    if (address != 0x827cd7bcu) return false;

    // Store order matters when full-width incoming registers/stack alias.
    const std::uint64_t incoming_sp = registers.sp;
    WriteU64(memory, Address(incoming_sp - 8u), registers.r31);
    registers.r31 = registers.r12 - 320u;
    WriteU64(memory, Address(incoming_sp - 16u), registers.r22);
    registers.r12 = registers.lr;
    memory.WriteU32(Address(incoming_sp - 24u),
        static_cast<std::uint32_t>(registers.r12));
    const std::uint64_t frame_sp = incoming_sp - 112u;
    memory.WriteU32(Address(frame_sp), static_cast<std::uint32_t>(incoming_sp));
    registers.sp = frame_sp;

    registers.r11 = memory.ReadU32(Address(registers.r31 + 96u));
    const bool nonzero = static_cast<std::uint32_t>(registers.r11) != 0u;
    registers.cr6 = {0, static_cast<std::uint8_t>(nonzero),
        static_cast<std::uint8_t>(!nonzero), registers.xer_so};
    if (nonzero)
    {
        registers.r3 = memory.ReadU32(Address(registers.r22 + 1408u));
        registers.lr = 0x827cd7e8u;
        native.LeaveCriticalSection(memory, registers);
    }

    // lwz r1 follows the live SP, then all restore slots follow the live,
    // zero-extended backchain. In particular this does not retain high SP bits.
    registers.sp = memory.ReadU32(Address(registers.sp));
    registers.r31 = ReadU64(memory, Address(registers.sp - 8u));
    registers.r22 = ReadU64(memory, Address(registers.sp - 16u));
    registers.r12 = memory.ReadU32(Address(registers.sp - 24u));
    registers.lr = registers.r12;
    return true;
}
} // namespace lo::semantic::gpu::heap_lock_exit
