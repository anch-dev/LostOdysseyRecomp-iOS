#include "lo_semantics/crt_stream_pointer_unlock.h"

#include <bit>

namespace lo::semantic::gpu::crt_stream_pointer_unlock
{
namespace
{
GuestAddress Address(std::uint64_t full)
{ return static_cast<GuestAddress>(full); }

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

void UnlockPointer(GuestMemory& memory, NativeServices& native,
    Registers& registers)
{
    const auto index = static_cast<std::int32_t>(registers.r3);
    registers.xer_ca = static_cast<std::uint8_t>(
        index < 0 && (static_cast<std::uint32_t>(index) & 31u) != 0);
    registers.r10 = static_cast<std::uint64_t>(index >> 5);
    registers.r11 = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(-2093481984));
    registers.r9 = std::rotl(static_cast<std::uint32_t>(registers.r10), 2) &
        0xfffffffcu;
    registers.r11 -= 29312u;
    registers.r10 = std::rotl(static_cast<std::uint32_t>(registers.r3), 6) &
        0x7c0u;
    registers.r11 = memory.ReadU32(Address(registers.r9 + registers.r11));
    registers.r11 += registers.r10;
    registers.r3 = registers.r11 + 12u;
    // b, not bl: the leaf preserves incoming LR and returns through native.
    native.LeaveCriticalSection(memory, registers);
}

void WrapPointer(GuestMemory& memory, NativeServices& native,
    Registers& registers, std::uint64_t return_address)
{
    const auto incoming_sp = registers.sp;
    WriteU64(memory, Address(incoming_sp - 8u), registers.r31);
    registers.r31 = registers.r12 - 160u;
    WriteU64(memory, Address(incoming_sp - 16u), registers.r30);
    registers.r12 = registers.lr;
    memory.WriteU32(Address(incoming_sp - 24u), Address(registers.r12));
    const auto frame_sp = incoming_sp - 112u;
    memory.WriteU32(Address(frame_sp), Address(incoming_sp));
    registers.sp = frame_sp;
    registers.r3 = registers.r30;
    registers.lr = return_address;
    UnlockPointer(memory, native, registers);
    registers.sp = memory.ReadU32(Address(registers.sp));
    registers.r31 = ReadU64(memory, Address(registers.sp - 8u));
    registers.r30 = ReadU64(memory, Address(registers.sp - 16u));
    registers.r12 = memory.ReadU32(Address(registers.sp - 24u));
    registers.lr = registers.r12;
}

void UnlockStreamField(GuestMemory& memory, NativeServices& native,
    Registers& registers)
{
    const auto incoming_sp = registers.sp;
    WriteU64(memory, Address(incoming_sp - 8u), registers.r30);
    registers.r12 = registers.lr;
    memory.WriteU32(Address(incoming_sp - 16u), Address(registers.r12));
    const auto frame_sp = incoming_sp - 96u;
    memory.WriteU32(Address(frame_sp), Address(incoming_sp));
    registers.sp = frame_sp;
    registers.r3 = memory.ReadU32(Address(registers.r30 + 80u));
    registers.lr = 0x82b81b10u;
    native.LeaveCriticalSection(memory, registers);
    registers.sp = memory.ReadU32(Address(registers.sp));
    registers.r30 = ReadU64(memory, Address(registers.sp - 8u));
    registers.r12 = memory.ReadU32(Address(registers.sp - 16u));
    registers.lr = registers.r12;
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& registers)
{
    switch (address)
    {
    case 0x82b863f0u:
        UnlockPointer(memory, native, registers);
        return true;
    case 0x82b81f38u:
        WrapPointer(memory, native, registers, 0x82b81f58u);
        return true;
    case 0x82b860ccu:
        WrapPointer(memory, native, registers, 0x82b860ecu);
        return true;
    case 0x82b81af8u:
        UnlockStreamField(memory, native, registers);
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::crt_stream_pointer_unlock
