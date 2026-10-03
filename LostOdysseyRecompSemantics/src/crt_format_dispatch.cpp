#include "lo_semantics/crt_format_dispatch.h"

#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/read_only_fields.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_format_dispatch
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

constexpr GuestAddress kDecimalPointPointer = 0x83215648u;
constexpr std::uint64_t kFormatterCodeBase = 0xffffffff82b80000ull;
constexpr std::uint64_t kDispatchDataBase = 0xffffffff83210000ull;
constexpr std::uint64_t kScientificCodeBase = 0xffffffff82320000ull;

void CompareSignedByte(Condition& cr, std::uint64_t value, std::uint8_t so)
{
    const auto signed_value = static_cast<std::int32_t>(value);
    cr = {static_cast<std::uint8_t>(signed_value < 0),
        static_cast<std::uint8_t>(signed_value > 0),
        static_cast<std::uint8_t>(signed_value == 0), so};
}

void CompareWords(Condition& cr, std::uint64_t left, std::uint64_t right,
    std::uint8_t so)
{
    const auto a = static_cast<std::int32_t>(left);
    const auto b = static_cast<std::int32_t>(right);
    cr = {static_cast<std::uint8_t>(a < b),
        static_cast<std::uint8_t>(a > b),
        static_cast<std::uint8_t>(a == b), so};
}

void CompareUnsignedWords(Condition& cr, std::uint64_t left,
    std::uint64_t right, std::uint8_t so)
{
    const auto a = static_cast<std::uint32_t>(left);
    const auto b = static_cast<std::uint32_t>(right);
    cr = {static_cast<std::uint8_t>(a < b),
        static_cast<std::uint8_t>(a > b),
        static_cast<std::uint8_t>(a == b), so};
}

std::uint64_t SignExtendByte(std::uint64_t value)
{
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(
        static_cast<std::int8_t>(value)));
}

void InitializeFloatDispatch(GuestMemory& memory, Registers& state)
{
    const auto sp = Address(state.sp);
    WriteU64(memory, sp - 16u, state.r[30]);
    WriteU64(memory, sp - 8u, state.r[31]);

    state.r[10] = kFormatterCodeBase;
    state.r[11] = kDispatchDataBase;
    state.r[10] -= 960u;
    state.r[11] += 20424u;
    state.r[30] = kFormatterCodeBase;
    state.r[31] = kFormatterCodeBase;
    state.r[3] = kFormatterCodeBase;
    state.r[4] = kFormatterCodeBase;
    memory.WriteU32(Address(state.r[11]), Address(state.r[10]));

    state.r[10] = state.r[30] - 4032u;
    state.r[5] = kFormatterCodeBase;
    state.r[6] = kScientificCodeBase;
    state.r[7] = kFormatterCodeBase;
    state.r[8] = kFormatterCodeBase;
    memory.WriteU32(Address(state.r[11] + 4u), Address(state.r[10]));
    state.r[10] = state.r[31] - 4048u;
    state.r[9] = kFormatterCodeBase;
    memory.WriteU32(Address(state.r[11] + 8u), Address(state.r[10]));
    state.r[10] = state.r[3] - 4040u;
    memory.WriteU32(Address(state.r[11] + 12u), Address(state.r[10]));
    state.r[10] = state.r[4] - 4176u;
    memory.WriteU32(Address(state.r[11] + 16u), Address(state.r[10]));
    state.r[10] = state.r[5] - 960u;
    memory.WriteU32(Address(state.r[11] + 20u), Address(state.r[10]));
    state.r[10] = state.r[6] - 23904u;
    memory.WriteU32(Address(state.r[11] + 24u), Address(state.r[10]));
    state.r[10] = state.r[7] - 4144u;
    memory.WriteU32(Address(state.r[11] + 28u), Address(state.r[10]));
    state.r[10] = state.r[8] - 4360u;
    memory.WriteU32(Address(state.r[11] + 32u), Address(state.r[10]));
    state.r[10] = state.r[9] - 4520u;
    memory.WriteU32(Address(state.r[11] + 36u), Address(state.r[10]));
    state.r[30] = ReadU64(memory, sp - 16u);
    state.r[31] = ReadU64(memory, sp - 8u);
}

void TrimFloatText(GuestMemory& memory, Registers& state)
{
    // Find the locale's decimal separator before examining the fraction.
    state.r[11] = memory.ReadU8(Address(state.r[3]));
    state.r[10] = kDispatchDataBase;
    state.r[8] = memory.ReadU32(kDecimalPointPointer);
    state.r[11] = SignExtendByte(state.r[11]);
    CompareSignedByte(state.cr0, state.r[11], state.xer_so);
    if (!state.cr0.eq)
    {
        state.r[10] = memory.ReadU32(Address(state.r[8]));
        state.r[10] = memory.ReadU8(Address(state.r[10]));
        state.r[10] = SignExtendByte(state.r[10]);
        for (;;)
        {
            CompareWords(state.cr6, state.r[11], state.r[10], state.xer_so);
            if (state.cr6.eq || state.cr0.eq)
                break;
            state.r[3] += 1u;
            state.r[11] = memory.ReadU8(Address(state.r[3]));
            state.r[11] = SignExtendByte(state.r[11]);
            CompareSignedByte(state.cr0, state.r[11], state.xer_so);
            if (state.cr0.eq)
                break;
        }
    }

    state.r[10] = memory.ReadU8(Address(state.r[3]));
    state.r[11] = state.r[3] + 1u;
    CompareUnsignedWords(state.cr6, state.r[10], 0, state.xer_so);
    if (state.cr6.eq)
        return;

    // The suffix begins at an exponent letter or the terminator. Work back
    // over fractional zeros, then compact the suffix over those bytes.
    for (;;)
    {
        state.r[10] = memory.ReadU8(Address(state.r[11]));
        state.r[10] = SignExtendByte(state.r[10]);
        CompareSignedByte(state.cr0, state.r[10], state.xer_so);
        if (state.cr0.eq)
            break;
        CompareWords(state.cr6, state.r[10], 'e', state.xer_so);
        if (state.cr6.eq)
            break;
        CompareWords(state.cr6, state.r[10], 'E', state.xer_so);
        if (state.cr6.eq)
            break;
        state.r[11] += 1u;
    }
    state.r[9] = state.r[11];
    do
    {
        state.r[11] -= 1u;
        state.r[10] = memory.ReadU8(Address(state.r[11]));
        CompareUnsignedWords(state.cr6, state.r[10], '0', state.xer_so);
    } while (state.cr6.eq);

    state.r[10] = memory.ReadU32(Address(state.r[8]));
    state.r[8] = memory.ReadU8(Address(state.r[11]));
    state.r[10] = memory.ReadU8(Address(state.r[10]));
    CompareUnsignedWords(state.cr6, state.r[8], state.r[10], state.xer_so);
    if (state.cr6.eq)
        state.r[11] -= 1u;

    do
    {
        state.r[10] = memory.ReadU8(Address(state.r[9]));
        state.r[11] += 1u;
        state.r[9] += 1u;
        CompareUnsignedWords(state.cr0, state.r[10], 0, state.xer_so);
        memory.WriteU8(Address(state.r[11]),
            static_cast<std::uint8_t>(state.r[10]));
    } while (!state.cr0.eq);
}

void LowerAscii(Registers& state)
{
    state.r[11] = state.r[3] - 65u;
    CompareUnsignedWords(state.cr6, state.r[11], 25u, state.xer_so);
    if (!state.cr6.gt)
        state.r[3] += 32u;
}

void CallCharacterFlag(GuestMemory& memory, Registers& state)
{
    read_only_fields::Registers call{
        state.r[3], state.r[4], state.r[5], state.r[8], state.r[9],
        state.r[10], state.r[11], state.r[13], state.r[18]};
    (void)read_only_fields::Apply(0x823588a0u, call, memory);
    state.r[3] = call.r3; state.r[4] = call.r4; state.r[5] = call.r5;
    state.r[8] = call.r8; state.r[9] = call.r9; state.r[10] = call.r10;
    state.r[11] = call.r11; state.r[13] = call.r13;
    state.r[18] = call.r18;
}

void NormalizeFloatDecimal(GuestMemory& memory, Registers& state)
{
    state.r[12] = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(state.r[12]));
    WriteU64(memory, Address(state.sp - 16u), state.r[31]);
    const auto caller_sp = Address(state.sp);
    state.sp -= 96u;
    memory.WriteU32(Address(state.sp), caller_sp);
    state.r[31] = state.r[3];

    state.r[11] = memory.ReadU8(Address(state.r[31]));
    state.r[3] = SignExtendByte(state.r[11]);
    state.lr = 0x82b7ee78u;
    LowerAscii(state);
    CompareWords(state.cr6, state.r[3], 'e', state.xer_so);
    if (!state.cr6.eq)
    {
        // 823588A0 is the accepted CRT character-table flag probe. Re-read
        // each live byte and carry through all of its selected GPR effects.
        do
        {
            state.r[31] += 1u;
            state.r[3] = memory.ReadU8(Address(state.r[31]));
            state.lr = 0x82b7ee8cu;
            CallCharacterFlag(memory, state);
            CompareWords(state.cr0, state.r[3], 0, state.xer_so);
        } while (!state.cr0.eq);
    }

    state.r[11] = memory.ReadU8(Address(state.r[31]));
    state.r[3] = SignExtendByte(state.r[11]);
    state.lr = 0x82b7eea0u;
    LowerAscii(state);
    CompareWords(state.cr6, state.r[3], 'x', state.xer_so);
    if (state.cr6.eq)
        state.r[31] += 2u;

    state.r[11] = kDispatchDataBase;
    state.r[10] = memory.ReadU8(Address(state.r[31]));
    state.r[11] = memory.ReadU32(kDecimalPointPointer);
    state.r[11] = memory.ReadU32(Address(state.r[11]));
    state.r[11] = memory.ReadU8(Address(state.r[11]));
    memory.WriteU8(Address(state.r[31]),
        static_cast<std::uint8_t>(state.r[11]));
    state.r[11] = state.r[31] + 1u;
    do
    {
        state.r[9] = memory.ReadU8(Address(state.r[11]));
        memory.WriteU8(Address(state.r[11]),
            static_cast<std::uint8_t>(state.r[10]));
        state.r[10] = static_cast<std::uint8_t>(state.r[10]);
        state.r[11] += 1u;
        CompareUnsignedWords(state.cr6, state.r[10], 0, state.xer_so);
        state.r[10] = state.r[9];
    } while (!state.cr6.eq);

    state.sp += 96u;
    state.r[12] = memory.ReadU32(Address(state.sp - 8u));
    state.lr = state.r[12];
    state.r[31] = ReadU64(memory, Address(state.sp - 16u));
}

crt_stream_locks::Registers ToLockRegisters(const Registers& state)
{
    crt_stream_locks::Registers call{};
    call.sp = state.sp; call.lr = state.lr; call.ctr = state.ctr;
    call.r3 = state.r[3]; call.r4 = state.r[4]; call.r5 = state.r[5];
    call.r6 = state.r[6]; call.r7 = state.r[7]; call.r8 = state.r[8];
    call.r9 = state.r[9]; call.r10 = state.r[10]; call.r11 = state.r[11];
    call.r12 = state.r[12]; call.r13 = state.r[13];
    call.r28 = state.r[28]; call.r29 = state.r[29];
    call.r30 = state.r[30]; call.r31 = state.r[31];
    call.cr0 = {state.cr0.lt != 0, state.cr0.gt != 0,
        state.cr0.eq != 0, state.cr0.so != 0};
    call.cr6 = {state.cr6.lt != 0, state.cr6.gt != 0,
        state.cr6.eq != 0, state.cr6.so != 0};
    call.xer_so = state.xer_so; call.xer_ca = state.xer_ca;
    return call;
}

void FromLockRegisters(Registers& state,
    const crt_stream_locks::Registers& call)
{
    state.sp = call.sp; state.lr = call.lr; state.ctr = call.ctr;
    state.r[3] = call.r3; state.r[4] = call.r4; state.r[5] = call.r5;
    state.r[6] = call.r6; state.r[7] = call.r7; state.r[8] = call.r8;
    state.r[9] = call.r9; state.r[10] = call.r10; state.r[11] = call.r11;
    state.r[12] = call.r12; state.r[13] = call.r13;
    state.r[28] = call.r28; state.r[29] = call.r29;
    state.r[30] = call.r30; state.r[31] = call.r31;
    state.cr0 = {static_cast<std::uint8_t>(call.cr0.lt),
        static_cast<std::uint8_t>(call.cr0.gt),
        static_cast<std::uint8_t>(call.cr0.eq),
        static_cast<std::uint8_t>(call.cr0.so)};
    state.cr6 = {static_cast<std::uint8_t>(call.cr6.lt),
        static_cast<std::uint8_t>(call.cr6.gt),
        static_cast<std::uint8_t>(call.cr6.eq),
        static_cast<std::uint8_t>(call.cr6.so)};
    state.xer_so = call.xer_so; state.xer_ca = call.xer_ca;
}

void MissingFloatSupport(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    state.r[3] = 2;
    auto call = ToLockRegisters(state);
    (void)crt_stream_locks::Apply(0x82b7bed8u, memory, dependencies, call);
    FromLockRegisters(state, call);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& state)
{
    switch (entry)
    {
    case 0x82b7a5e0u:
    case 0x82b7a678u:
        InitializeFloatDispatch(memory, state);
        return true;
    case 0x82b7f040u:
        state.r[4] = 0;
        [[fallthrough]];
    case 0x82b7eef8u:
        TrimFloatText(memory, state);
        return true;
    case 0x82b833d0u:
        LowerAscii(state);
        return true;
    case 0x82b7f038u:
        state.r[4] = 0;
        [[fallthrough]];
    case 0x82b7ee58u:
        NormalizeFloatDecimal(memory, state);
        return true;
    case 0x82b84c90u:
        MissingFloatSupport(memory, dependencies, state);
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::crt_format_dispatch
