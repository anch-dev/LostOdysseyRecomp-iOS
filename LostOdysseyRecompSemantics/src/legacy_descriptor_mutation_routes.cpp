#include "lo_semantics/legacy_descriptor_mutation_routes.h"

#include "lo_semantics/legacy_descriptor_record_routes.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_mutation_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index)
{ return index == 1u ? state.sp : state.r[index]; }

void CompareU(Registers& state, std::uint64_t left,
    std::uint64_t right, Condition& cr)
{
    const auto l = static_cast<std::uint32_t>(left);
    const auto r = static_cast<std::uint32_t>(right);
    cr = {std::uint8_t(l < r), std::uint8_t(l > r),
        std::uint8_t(l == r), state.xer_so};
}

void CompareS(Registers& state, std::uint64_t left,
    std::uint64_t right, Condition& cr)
{
    const auto l = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    const auto r = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(right));
    cr = {std::uint8_t(l < r), std::uint8_t(l > r),
        std::uint8_t(l == r), state.xer_so};
}

void Rlwimi(std::uint64_t& destination, std::uint64_t source,
    int rotation, std::uint64_t mask)
{ destination = WordRotateMask(source, rotation, mask) |
    (destination & ~mask); }

void ReadWord(GuestMemory& memory, Registers& state, unsigned target,
    std::uint64_t address)
{ R(state, target) = memory.ReadU32(Address(address)); }

void WriteWord(GuestMemory& memory, std::uint64_t address,
    std::uint64_t value)
{ memory.WriteU32(Address(address), static_cast<std::uint32_t>(value)); }

void SelectRecord(GuestMemory& memory, Registers& state)
{ (void)legacy_descriptor_record_routes::Apply(0x82fac238u, memory, state); }

void Save96(GuestMemory& memory, Registers& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    WriteWord(memory, incoming_sp - 8u, R(state, 12));
    WriteU64(memory, Address(incoming_sp - 16u), R(state, 31));
    WriteWord(memory, incoming_sp - 96u, incoming_sp);
    state.sp -= 96u;
}

void Restore96(GuestMemory& memory, Registers& state)
{
    state.sp += 96u;
    ReadWord(memory, state, 12, state.sp - 8u);
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void WriteRequestedFlags(GuestMemory& memory, Registers& state,
    std::uint32_t bits, bool word_mask)
{
    ReadWord(memory, state, 11, R(state, 31) + 16u);
    R(state, 10) = word_mask ? WordRotateMask(R(state, 11), 0, bits) :
        (R(state, 11) & bits);
    if (!word_mask)
        CompareS(state, R(state, 10), 0u, state.cr0);
    CompareU(state, R(state, 10), bits, state.cr6);
    if (!state.cr6.eq)
    {
        R(state, 11) |= bits;
        WriteWord(memory, R(state, 31) + 16u, R(state, 11));
    }
}

void Mutate(GuestMemory& memory, Registers& state)
{
    Save96(memory, state);
    Rlwimi(R(state, 8), R(state, 7), 5, 0xe0u);
    R(state, 31) = R(state, 3);
    R(state, 9) = R(state, 6);
    R(state, 10) = R(state, 6) - 5u;
    R(state, 3) = R(state, 4);
    CompareU(state, R(state, 10), 96u, state.cr6);
    ReadWord(memory, state, 11, R(state, 31) + 8u);
    WriteWord(memory, R(state, 31) + 24u, R(state, 5));
    R(state, 11) = WordRotateMask(R(state, 11), 0,
        0xfffffffffffe007full);
    R(state, 11) = WordRotateMask(R(state, 11), 0,
        0xffffffffffc7ffffull);
    R(state, 8) &= 231u;
    CompareS(state, R(state, 8), 0u, state.cr0);
    Rlwimi(R(state, 9), R(state, 8), 7, 0xffffff80u);
    R(state, 9) = WordRotateMask(R(state, 9), 7, 0xffffff80u);
    R(state, 11) = R(state, 9) | R(state, 11);
    WriteWord(memory, R(state, 31) + 8u, R(state, 11));
    if (!state.cr6.gt)
    {
        R(state, 12) = static_cast<std::uint64_t>(
            std::int64_t{-2112421888} + 20040);
        R(state, 0) = memory.ReadU8(Address(R(state, 12) + R(state, 10)));
        R(state, 12) = static_cast<std::uint64_t>(
            std::int64_t{-2096824320} + 26080);
        R(state, 12) += R(state, 0);
        state.ctr = R(state, 12);
        const auto index = static_cast<std::uint32_t>(R(state, 10));
        if (index == 0u || index == 1u || index == 2u || index == 3u ||
            (index >= 20u && index <= 23u) ||
            (index >= 33u && index <= 36u) ||
            (index >= 61u && index <= 65u))
            WriteRequestedFlags(memory, state, 3510u, false);
        else if (index == 5u || index == 6u || index == 38u ||
            index == 39u)
            WriteRequestedFlags(memory, state, 2340u, false);
        else if (index == 24u)
            WriteRequestedFlags(memory, state, 6u, true);
        else if ((index >= 46u && index <= 48u) || index == 66u)
            WriteRequestedFlags(memory, state, 1170u, false);
        else if (index == 80u || index == 81u)
        {
            ReadWord(memory, state, 11, R(state, 3) + 40u);
            R(state, 11) |= 1024u;
            WriteWord(memory, R(state, 3) + 40u, R(state, 11));
        }
        else if (index == 90u || index == 91u || index == 93u ||
            index == 94u)
        {
            R(state, 6) = WordRotateMask(R(state, 11), 18, 7u);
            R(state, 5) = WordRotateMask(R(state, 11), 13, 7u);
            R(state, 4) = WordRotateMask(R(state, 11), 25, 0x7fu);
            state.lr = index == 90u ? 0x83056664u : 0x83056690u;
            SelectRecord(memory, state);
            R(state, 11) = R(state, 31) - (index == 90u ? 20u : 24u);
            R(state, 10) = index == 90u ? 57u : 228u;
            R(state, 11) = R(state, 3) + R(state, 11);
            if (index == 90u)
            {
                ReadWord(memory, state, 9, R(state, 11) + 12u);
                Rlwimi(R(state, 9), R(state, 10), 19, 0x1fe0000u);
                WriteWord(memory, R(state, 11) + 12u, R(state, 9));
            }
            else
            {
                ReadWord(memory, state, 8, R(state, 11) + 4u);
                ReadWord(memory, state, 9, R(state, 11));
                memory.WriteU8(Address(R(state, 11) + 14u),
                    static_cast<std::uint8_t>(R(state, 10)));
                R(state, 10) = R(state, 8) | 253689856u;
                R(state, 9) |= 524288u;
                R(state, 10) |= 61440u;
                WriteWord(memory, R(state, 11), R(state, 9));
                WriteWord(memory, R(state, 11) + 4u, R(state, 10));
            }
        }
        else if ((index >= 16u && index <= 19u) || index == 25u ||
            (index >= 49u && index <= 50u) ||
            (index >= 53u && index <= 60u) ||
            (index >= 85u && index <= 88u) || index == 92u ||
            index == 95u || index == 96u)
        {
            R(state, 11) |= 64u;
            WriteWord(memory, R(state, 31) + 8u, R(state, 11));
        }
    }
    Restore96(memory, state);
}

void Save128(GuestMemory& memory, Registers& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    state.lr = 0x83056ac8u;
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(memory, Address(incoming_sp - 48u + (index - 27u) * 8u),
            R(state, index));
    WriteWord(memory, incoming_sp - 8u, R(state, 12));
    WriteWord(memory, incoming_sp - 128u, incoming_sp);
    state.sp -= 128u;
}

void Restore128(GuestMemory& memory, Registers& state)
{
    state.sp += 128u;
    for (unsigned index = 27u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 48u + (index - 27u) * 8u));
    ReadWord(memory, state, 12, state.sp - 8u);
    state.lr = R(state, 12);
}

void MutateCaller(GuestMemory& memory, Registers& state)
{
    Save128(memory, state);
    R(state, 11) = R(state, 5);
    R(state, 5) = R(state, 6);
    R(state, 10) = R(state, 11) - 103u;
    R(state, 6) = R(state, 11);
    R(state, 11) = std::countl_zero(
        static_cast<std::uint32_t>(R(state, 10)));
    R(state, 28) = R(state, 7);
    R(state, 7) = WordRotateMask(R(state, 11), 27, 1u);
    R(state, 30) = R(state, 8);
    ReadWord(memory, state, 8, state.sp + 212u);
    R(state, 31) = R(state, 3);
    R(state, 29) = R(state, 4);
    R(state, 27) = R(state, 9);
    state.lr = 0x83056b00u;
    Mutate(memory, state);
    ReadWord(memory, state, 11, R(state, 31) + 8u);
    R(state, 3) = R(state, 29);
    R(state, 6) = WordRotateMask(R(state, 11), 18, 7u);
    R(state, 5) = WordRotateMask(R(state, 11), 13, 7u);
    R(state, 4) = WordRotateMask(R(state, 11), 25, 0x7fu);
    state.lr = 0x83056b18u;
    SelectRecord(memory, state);
    R(state, 11) = R(state, 31) - 8u;
    ReadWord(memory, state, 10, state.sp + 220u);
    R(state, 11) = R(state, 3) + R(state, 11);
    Rlwimi(R(state, 30), R(state, 10), 14, 0xc000u);
    CompareS(state, R(state, 10), 2u, state.cr6);
    ReadWord(memory, state, 9, R(state, 11));
    ReadWord(memory, state, 8, R(state, 11) + 4u);
    Rlwimi(R(state, 9), R(state, 30), 15, 0x7fff8000u);
    Rlwimi(R(state, 8), R(state, 27), 0, 0xfu);
    Rlwimi(R(state, 9), R(state, 28), 0, 0x7fffu);
    R(state, 8) |= 1048576u;
    WriteWord(memory, R(state, 11), R(state, 9));
    WriteWord(memory, R(state, 11) + 4u, R(state, 8));
    if (state.cr6.eq) WriteRequestedFlags(memory, state, 2340u, false);
    else
    {
        CompareS(state, R(state, 10), 3u, state.cr6);
        if (state.cr6.eq)
            WriteRequestedFlags(memory, state, 3510u, false);
    }
    Restore128(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    switch (entry)
    {
    case 0x83056568u: Mutate(memory, registers); return true;
    case 0x83056ac0u: MutateCaller(memory, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_mutation_routes
