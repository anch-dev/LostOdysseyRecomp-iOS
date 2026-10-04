#include "lo_semantics/legacy_descriptor_record_routes.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_record_routes
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

void CompareS(Registers& state, std::uint64_t value, Condition& cr)
{
    const auto signed_value = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(value));
    cr = {std::uint8_t(signed_value < 0),
        std::uint8_t(signed_value > 0), std::uint8_t(signed_value == 0),
        state.xer_so};
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
{
    R(state, 11) = R(state, 4) - 83u;
    R(state, 10) = R(state, 5) + 10u;
    CompareU(state, R(state, 11), 41u, state.cr6);
    R(state, 3) = WordRotateMask(R(state, 10), 2, 0xfffffffcu);
    if (state.cr6.gt) return;
    R(state, 12) = static_cast<std::uint64_t>(
        std::int64_t{-2112421888} + 4768);
    R(state, 0) = memory.ReadU8(Address(R(state, 12) + R(state, 11)));
    R(state, 12) = static_cast<std::uint64_t>(
        std::int64_t{-2097479680} - 15756);
    R(state, 12) += R(state, 0);
    state.ctr = R(state, 12);
    // The generated PPC body dispatches on the index after recording the
    // image-table byte in r0/r12/CTR, so retain that exact control flow.
    switch (static_cast<std::uint32_t>(R(state, 11)))
    {
    case 41u:
        R(state, 11) = WordRotateMask(R(state, 6), 2, 0xfffffffcu);
        R(state, 3) = R(state, 11) + R(state, 3);
        break;
    case 2u: case 3u: case 5u: case 6u: case 35u:
        R(state, 3) += 12u; break;
    case 20u: case 28u: case 33u: case 40u:
        R(state, 3) += 8u; break;
    case 12u: R(state, 3) += 20u; break;
    case 13u: case 15u: case 16u:
        R(state, 3) += 24u; break;
    case 0u: R(state, 3) += 28u; break;
    case 1u: R(state, 3) += 16u; break;
    case 10u: case 19u: case 32u:
        R(state, 3) += 4u; break;
    default: break;
    }
}

void SaveFrame(GuestMemory& memory, Registers& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    WriteWord(memory, incoming_sp - 8u, R(state, 12));
    WriteU64(memory, Address(incoming_sp - 24u), R(state, 30));
    WriteU64(memory, Address(incoming_sp - 16u), R(state, 31));
    WriteWord(memory, incoming_sp - 112u, incoming_sp);
    state.sp -= 112u;
}

void RestoreFrame(GuestMemory& memory, Registers& state)
{
    state.sp += 112u;
    ReadWord(memory, state, 12, state.sp - 8u);
    state.lr = R(state, 12);
    R(state, 30) = ReadU64(memory, Address(state.sp - 24u));
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

// Insert one of the record's four 3-bit state values into output word 1.
// The caller retains the exact PPC scratch-register and CR updates.
void InsertRecordNibble(GuestMemory& memory, Registers& state,
    std::uint32_t code, std::uint32_t position)
{
    ReadWord(memory, state, 11, R(state, 31) + 4u);
    R(state, 10) = code;
    static constexpr int OneRotations[] = {2, 5, 8, 11};
    static constexpr int FiveRotations[] = {0, 3, 6, 9};
    static constexpr std::uint64_t Masks[] = {0x7u, 0x38u,
        0x1c0u, 0xe00u};
    const auto slot = position < 4u ? position : 0u;
    Rlwimi(R(state, 11), R(state, 10),
        code == 1u ? OneRotations[slot] : FiveRotations[slot],
        Masks[slot]);
    WriteWord(memory, R(state, 31) + 4u, R(state, 11));
}

void BuildRecord(GuestMemory& memory, Registers& state)
{
    SaveFrame(memory, state);
    R(state, 30) = R(state, 3);
    R(state, 31) = R(state, 4);
    R(state, 3) = R(state, 5);
    ReadWord(memory, state, 11, R(state, 30) + 8u);
    R(state, 6) = WordRotateMask(R(state, 11), 18, 7u);
    R(state, 5) = WordRotateMask(R(state, 11), 13, 7u);
    R(state, 4) = WordRotateMask(R(state, 11), 25, 0x7fu);
    state.lr = 0x83055614u;
    SelectRecord(memory, state);
    R(state, 11) = R(state, 30) - 20u;
    R(state, 9) = R(state, 3) + R(state, 11);
    for (unsigned offset = 0u; offset < 12u; offset += 4u)
    {
        ReadWord(memory, state, 11, R(state, 9) + offset);
        WriteWord(memory, R(state, 31) + offset, R(state, 11));
    }
    ReadWord(memory, state, 11, R(state, 30) + 4u);
    while (true)
    {
        CompareU(state, R(state, 11), 0u, state.cr6);
        if (state.cr6.eq) break;
        ReadWord(memory, state, 10, R(state, 11) + 16u);
        CompareU(state, R(state, 10), 0u, state.cr6);
        if (!state.cr6.eq)
        {
            ReadWord(memory, state, 10, R(state, 11));
            R(state, 10) = WordRotateMask(R(state, 10), 0, 0x0e000000u);
            CompareS(state, R(state, 10), state.cr0);
            if (!state.cr0.eq) break;
        }
        ReadWord(memory, state, 11, R(state, 11) + 8u);
    }
    if (state.cr6.eq)
    {
        ReadWord(memory, state, 11, R(state, 31) + 4u);
        R(state, 11) |= 4095u;
        WriteWord(memory, R(state, 31) + 4u, R(state, 11));
    }
    else
    {
        ReadWord(memory, state, 10, R(state, 11));
        ReadWord(memory, state, 8, R(state, 31));
        Rlwimi(R(state, 8), R(state, 10), 27, 0x3f000u);
        WriteWord(memory, R(state, 31), R(state, 8));
        ReadWord(memory, state, 11, R(state, 11));
        R(state, 11) = WordRotateMask(R(state, 11), 0, 0x10u);
        CompareS(state, R(state, 11), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 11) = WordRotateMask(R(state, 8), 0, 0xffffffffu);
            R(state, 11) |= 262144u;
            WriteWord(memory, R(state, 31), R(state, 11));
        }
        ReadWord(memory, state, 11, R(state, 30) + 8u);
        ReadWord(memory, state, 8, R(state, 9) + 12u);
        R(state, 10) = WordRotateMask(R(state, 11), 31, 0xfu);
        R(state, 11) = WordRotateMask(R(state, 8), 15, 0xffu);
        R(state, 8) = static_cast<std::uint32_t>(R(state, 10)) & 1u;
        CompareS(state, R(state, 8), state.cr0);
        ReadWord(memory, state, 8, R(state, 31) + 4u);
        if (!state.cr0.eq)
        {
            R(state, 7) = static_cast<std::uint32_t>(R(state, 11)) & 3u;
            R(state, 8) = WordRotateMask(R(state, 8), 0, 0xfffffff8u);
            R(state, 8) |= R(state, 7);
        }
        else R(state, 8) |= 7u;
        WriteWord(memory, R(state, 31) + 4u, R(state, 8));
        R(state, 8) = WordRotateMask(R(state, 10), 0, 2u);
        CompareS(state, R(state, 8), state.cr0);
        ReadWord(memory, state, 8, R(state, 31) + 4u);
        if (!state.cr0.eq)
        {
            Rlwimi(R(state, 8), R(state, 11), 1, 0x18u);
            R(state, 8) = WordRotateMask(R(state, 8), 0,
                0xffffffffffffffdfull);
        }
        else R(state, 8) |= 56u;
        WriteWord(memory, R(state, 31) + 4u, R(state, 8));
        R(state, 8) = WordRotateMask(R(state, 10), 0, 4u);
        CompareS(state, R(state, 8), state.cr0);
        ReadWord(memory, state, 8, R(state, 31) + 4u);
        if (!state.cr0.eq)
        {
            Rlwimi(R(state, 8), R(state, 11), 2, 0xc0u);
            R(state, 8) = WordRotateMask(R(state, 8), 0,
                0xfffffffffffffeffull);
        }
        else R(state, 8) |= 448u;
        WriteWord(memory, R(state, 31) + 4u, R(state, 8));
        R(state, 10) = WordRotateMask(R(state, 10), 0, 8u);
        CompareS(state, R(state, 10), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 10) = WordRotateMask(R(state, 8), 0, 0xffffffffu);
            Rlwimi(R(state, 10), R(state, 11), 3, 0x600u);
            R(state, 11) = WordRotateMask(R(state, 10), 0,
                0xfffffffffffff7ffull);
        }
        else
        {
            ReadWord(memory, state, 11, R(state, 31) + 4u);
            R(state, 11) |= 3584u;
        }
        WriteWord(memory, R(state, 31) + 4u, R(state, 11));
        ReadWord(memory, state, 11, R(state, 9) + 12u);
        R(state, 11) = WordRotateMask(R(state, 11), 24, 0xfu);
        CompareS(state, R(state, 11), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 11) = std::countl_zero(static_cast<std::uint32_t>(R(state, 11)));
            state.xer_ca = static_cast<std::uint32_t>(R(state, 11)) <= 31u;
            R(state, 11) = 31u - R(state, 11);
            CompareU(state, R(state, 11), 1u, state.cr6);
            if (!state.cr6.lt && !state.cr6.eq)
                CompareU(state, R(state, 11), 3u, state.cr6);
            if (static_cast<std::uint32_t>(R(state, 11)) <= 3u)
                InsertRecordNibble(memory, state, 1u,
                    static_cast<std::uint32_t>(R(state, 11)));
        }
        ReadWord(memory, state, 11, R(state, 9) + 12u);
        R(state, 11) = WordRotateMask(R(state, 11), 20, 0xfu);
        CompareS(state, R(state, 11), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 11) = std::countl_zero(static_cast<std::uint32_t>(R(state, 11)));
            state.xer_ca = static_cast<std::uint32_t>(R(state, 11)) <= 31u;
            R(state, 11) = 31u - R(state, 11);
            CompareU(state, R(state, 11), 1u, state.cr6);
            if (!state.cr6.lt && !state.cr6.eq)
                CompareU(state, R(state, 11), 3u, state.cr6);
            if (static_cast<std::uint32_t>(R(state, 11)) <= 3u)
                InsertRecordNibble(memory, state, 5u,
                    static_cast<std::uint32_t>(R(state, 11)));
        }
    }
    ReadWord(memory, state, 11, R(state, 30) + 8u);
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x380000u);
    CompareS(state, R(state, 11), state.cr0);
    if (!state.cr0.eq)
    {
        ReadWord(memory, state, 11, R(state, 30) + 40u);
        ReadWord(memory, state, 10, R(state, 31));
        ReadWord(memory, state, 9, R(state, 11));
        Rlwimi(R(state, 10), R(state, 9), 20, 0x7e0u);
        WriteWord(memory, R(state, 31), R(state, 10));
        ReadWord(memory, state, 10, R(state, 11));
        ReadWord(memory, state, 9, R(state, 31));
        Rlwimi(R(state, 9), R(state, 10), 25, 0xc0000000u);
        WriteWord(memory, R(state, 31), R(state, 9));
        ReadWord(memory, state, 11, R(state, 11));
        R(state, 11) = WordRotateMask(R(state, 11), 0, 0x10u);
        CompareS(state, R(state, 11), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 11) = WordRotateMask(R(state, 9), 0, 0xffffffffu);
            R(state, 11) |= 2048u;
            WriteWord(memory, R(state, 31), R(state, 11));
        }
    }
    ReadWord(memory, state, 11, R(state, 30) + 8u);
    R(state, 10) = 524288u;
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x380000u);
    CompareU(state, R(state, 11), R(state, 10), state.cr6);
    if (state.cr6.gt)
    {
        ReadWord(memory, state, 11, R(state, 30) + 44u);
        R(state, 10) = 3u;
        ReadWord(memory, state, 9, R(state, 31));
        ReadWord(memory, state, 11, R(state, 11));
        R(state, 11) = WordRotateMask(R(state, 11), 15, 0xffu);
        state.xer_ca = static_cast<std::uint32_t>(R(state, 11)) <= 95u;
        R(state, 11) = 95u - R(state, 11);
        const auto quotient = static_cast<std::uint32_t>(R(state, 11)) / 3u;
        R(state, 8) = (R(state, 8) & 0xffffffff00000000ull) | quotient;
        R(state, 10) = (R(state, 10) & 0xffffffff00000000ull) | quotient;
        R(state, 8) *= 3u;
        R(state, 11) -= R(state, 8);
        Rlwimi(R(state, 10), R(state, 11), 5, 0x60u);
        Rlwimi(R(state, 9), R(state, 10), 20, 0x7f00000u);
        WriteWord(memory, R(state, 31), R(state, 9));
    }
    else
    {
        ReadWord(memory, state, 11, R(state, 31));
        R(state, 10) = 95u;
        Rlwimi(R(state, 11), R(state, 10), 20, 0x7f00000u);
        WriteWord(memory, R(state, 31), R(state, 11));
    }
    ReadWord(memory, state, 11, R(state, 30) + 24u);
    ReadWord(memory, state, 10, R(state, 31) + 8u);
    ReadWord(memory, state, 11, R(state, 11) + 76u);
    R(state, 11) = WordRotateMask(R(state, 11), 10, 1u);
    CompareS(state, R(state, 11), state.cr0);
    if (!state.cr0.eq)
    {
        ReadWord(memory, state, 11, R(state, 31) + 4u);
        R(state, 11) |= 2147483648u;
        WriteWord(memory, R(state, 31) + 4u, R(state, 11));
        ReadWord(memory, state, 11, R(state, 30) + 24u);
        ReadWord(memory, state, 11, R(state, 11) + 76u);
        R(state, 11) = WordRotateMask(R(state, 11), 9, 0x1ffu);
        R(state, 11) = static_cast<std::uint32_t>(R(state, 11)) & 0xffu;
        Rlwimi(R(state, 10), R(state, 11), 31, 0x80000000u);
        WriteWord(memory, R(state, 31) + 8u, R(state, 10));
    }
    else
    {
        ReadWord(memory, state, 11, R(state, 30) + 8u);
        ReadWord(memory, state, 9, R(state, 31) + 4u);
        Rlwimi(R(state, 10), R(state, 11), 13, 0x80000000u);
        WriteWord(memory, R(state, 31) + 8u, R(state, 10));
        ReadWord(memory, state, 11, R(state, 30) + 8u);
        Rlwimi(R(state, 9), R(state, 11), 14, 0x80000000u);
        WriteWord(memory, R(state, 31) + 4u, R(state, 9));
    }
    RestoreFrame(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    switch (entry)
    {
    case 0x82fac238u: SelectRecord(memory, registers); return true;
    case 0x830555e0u: BuildRecord(memory, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_record_routes
