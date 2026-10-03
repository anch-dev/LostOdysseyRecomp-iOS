#include "lo_semantics/crt_stream_index_unlock.h"

namespace lo::semantic::gpu::crt_stream_index_unlock
{
namespace
{
constexpr GuestAddress kIndexedCriticalSections = 0x83215358u;
constexpr std::uint64_t kWrapperR11 = 0xffffffff83378d80ull;

GuestAddress Address(std::uint64_t value)
{ return static_cast<GuestAddress>(value); }

void LeaveIndexed(GuestMemory& memory, NativeServices& native,
    Registers& registers)
{
    // rlwinm 3,0,28 leaves the index multiplied by eight in the low word.
    registers.r11 = 0xffffffff83215358ull;
    registers.r10 = (Address(registers.r3) << 3u) & 0xfffffff8u;
    registers.r3 = memory.ReadU32(kIndexedCriticalSections + Address(registers.r10));
    native.LeaveCriticalSection(memory, registers);
}
} // namespace

bool Apply(GuestAddress address, GuestMemory& memory,
    NativeServices& native, Registers& registers)
{
    if (address == 0x82b819c8u)
    {
        LeaveIndexed(memory, native, registers);
        return true;
    }
    if (address != 0x82b863b8u) return false;

    const auto incoming_sp = registers.sp;
    registers.r12 = registers.lr;
    memory.WriteU32(Address(incoming_sp - 8u), Address(registers.r12));
    memory.WriteU32(Address(incoming_sp - 96u), Address(incoming_sp));
    registers.sp = incoming_sp - 96u;
    registers.r3 = 10;
    registers.lr = 0x82b863ccu;
    LeaveIndexed(memory, native, registers);

    registers.r11 = kWrapperR11;
    registers.r3 = memory.ReadU32(Address(registers.r31 + 148u));
    registers.r29 = memory.ReadU32(Address(registers.r31 + 80u));
    registers.sp = memory.ReadU32(Address(registers.sp));
    registers.r12 = memory.ReadU32(Address(registers.sp - 8u));
    registers.lr = registers.r12;
    return true;
}
} // namespace lo::semantic::gpu::crt_stream_index_unlock
