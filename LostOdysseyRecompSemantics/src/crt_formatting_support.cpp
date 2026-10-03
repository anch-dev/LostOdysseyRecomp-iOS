#include "lo_semantics/crt_formatting_support.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::crt_formatting_support
{
namespace
{
using namespace recovery_abi;

template<class T>
Condition CompareZero(T value, std::uint8_t overflow)
{
    return {static_cast<std::uint8_t>(value < T{0}),
        static_cast<std::uint8_t>(value > T{0}),
        static_cast<std::uint8_t>(value == T{0}), overflow};
}

void ReadByteCodeUnit(GuestMemory& memory, Registers& state)
{
    state.cr6 = CompareZero(Address(state.r4), state.xer_so);
    if (state.cr6.eq)
    {
        state.r3 = 0;
        return;
    }
    state.cr6 = CompareZero(Address(state.r5), state.xer_so);
    if (state.cr6.eq)
    {
        state.r3 = 0;
        return;
    }

    state.r11 = memory.ReadU8(Address(state.r4));
    state.cr6 = CompareZero(Address(state.r3), state.xer_so);
    state.cr0 = CompareZero(Address(state.r11), state.xer_so);
    if (!state.cr6.eq)
        memory.WriteU16(Address(state.r3), static_cast<std::uint16_t>(state.r11));
    state.r3 = state.cr0.eq ? 0u : 1u;
}

void MatchesTaggedGlobal(const GuestMemory& memory, Registers& state)
{
    state.r10 = std::uint64_t{memory.ReadU32(0x83216000u)} | 1u;
    const auto expected = memory.ReadU32(0x832D3CE8u);
    const auto difference = static_cast<std::uint32_t>(state.r10 - expected);
    state.r11 = static_cast<std::uint64_t>(std::countl_zero(difference));
    state.r3 = WordRotateMask(state.r11, 27, 1);
}

void OutputUnicodeError(GuestMemory& memory, NativeServices& services,
    Registers& state)
{
    state.r12 = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r12));
    WriteU64(memory, Address(state.sp - 16u), state.r31);
    memory.WriteU32(Address(state.sp - 112u), Address(state.sp));
    state.sp -= 112u;

    state.r4 = state.r3;
    state.r3 = state.sp + 88u;
    state.lr = 0x82BE471Cu;
    services.InitializeUnicodeString(memory, state);

    state.r5 = 1;
    state.r4 = state.sp + 88u;
    state.r3 = state.sp + 80u;
    state.lr = 0x82BE472Cu;
    services.UnicodeStringToAnsiString(memory, state);
    state.r31 = state.r3;
    state.cr0 = CompareZero(static_cast<std::int32_t>(Address(state.r31)),
        state.xer_so);
    if (state.cr0.lt)
    {
        state.r11 = 0xFFFFFFFF821A8F04ull;
        memory.WriteU32(Address(state.sp + 84u), Address(state.r11));
    }

    state.r3 = memory.ReadU32(Address(state.sp + 84u));
    state.lr = 0x82BE4748u;
    state.r3 = OutputCrtErrorMessage(memory, services, Address(state.r3),
        Address(state.sp));
    state.cr6 = CompareZero(static_cast<std::int32_t>(Address(state.r31)),
        state.xer_so);
    if (!state.cr6.lt)
    {
        state.r3 = state.sp + 80u;
        state.lr = 0x82BE4758u;
        services.FreeAnsiString(memory, state);
    }

    state.sp += 112u;
    state.r12 = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r12;
    state.r31 = ReadU64(memory, Address(state.sp - 16u));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, NativeServices& services,
    Registers& state)
{
    switch (entry)
    {
    case 0x822A07A0u: ReadByteCodeUnit(memory, state); return true;
    case 0x82B85420u: MatchesTaggedGlobal(memory, state); return true;
    case 0x82BE4700u: OutputUnicodeError(memory, services, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_formatting_support
