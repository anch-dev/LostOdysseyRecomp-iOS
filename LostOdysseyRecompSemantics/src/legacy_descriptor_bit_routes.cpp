#include "lo_semantics/legacy_descriptor_bit_routes.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_bit_routes
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

void CompareU(Condition& cr, std::uint64_t left,
    std::uint32_t right, std::uint8_t so)
{
    const auto value = static_cast<std::uint32_t>(left);
    cr = {std::uint8_t(value < right), std::uint8_t(value > right),
        std::uint8_t(value == right), so};
}

void CompareS(Condition& cr, std::uint64_t left,
    std::uint64_t right, std::uint8_t so)
{
    const auto l = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    const auto r = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(right));
    cr = {std::uint8_t(l < r), std::uint8_t(l > r),
        std::uint8_t(l == r), so};
}

void CompareWordsU(Condition& cr, std::uint64_t left,
    std::uint64_t right, std::uint8_t so)
{ CompareU(cr, left, static_cast<std::uint32_t>(right), so); }

void Rlwimi(std::uint64_t& destination, std::uint64_t source,
    int rotation, std::uint64_t mask)
{ destination = WordRotateMask(source, rotation, mask) |
    (destination & ~mask); }

std::uint64_t Slw(std::uint64_t value, std::uint64_t shift)
{
    const auto amount = static_cast<std::uint8_t>(shift);
    return (amount & 0x20u) ? 0u :
        static_cast<std::uint32_t>(value) << (amount & 0x3fu);
}

std::uint64_t Srw(std::uint64_t value, std::uint64_t shift)
{
    const auto amount = static_cast<std::uint8_t>(shift);
    return (amount & 0x20u) ? 0u :
        static_cast<std::uint32_t>(value) >> (amount & 0x3fu);
}

std::uint64_t Srd(std::uint64_t value, std::uint64_t shift)
{
    const auto amount = static_cast<std::uint8_t>(shift);
    return (amount & 0x40u) ? 0u : value >> (amount & 0x7fu);
}

std::uint64_t Cntlzw(std::uint64_t value)
{
    return std::countl_zero(static_cast<std::uint32_t>(value));
}

void Classify(GuestMemory& memory, Registers& state)
{
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 8u));
    R(state, 11) = WordRotateMask(R(state, 11), 25, 0x7fu);
    CompareU(state.cr6, R(state, 11), 31u, state.xer_so);
    if (!state.cr6.lt)
    {
        CompareU(state.cr6, R(state, 11), 81u, state.xer_so);
        R(state, 10) = state.cr6.gt ? 0u : 1u;
    }
    else R(state, 10) = 0u;
    R(state, 10) = static_cast<std::uint32_t>(R(state, 10)) & 0xffu;
    CompareS(state.cr0, R(state, 10), 0u, state.xer_so);
    if (!state.cr0.eq) { R(state, 3) = 1u; return; }
    CompareU(state.cr6, R(state, 11), 16u, state.xer_so);
    if (!state.cr6.lt)
    {
        CompareU(state.cr6, R(state, 11), 18u, state.xer_so);
        if (!state.cr6.gt) { R(state, 3) = 1u; return; }
        R(state, 11) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(R(state, 11)) - 20);
        CompareU(state.cr6, R(state, 11), 4u, state.xer_so);
        if (!state.cr6.gt) { R(state, 3) = 1u; return; }
    }
    R(state, 3) = 0u;
}

void SaveFrame(GuestMemory& memory, Registers& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    state.lr = 0x83054398u;
    WriteU64(memory, Address(incoming_sp - 40u), R(state, 28));
    WriteU64(memory, Address(incoming_sp - 32u), R(state, 29));
    WriteU64(memory, Address(incoming_sp - 24u), R(state, 30));
    WriteU64(memory, Address(incoming_sp - 16u), R(state, 31));
    memory.WriteU32(Address(incoming_sp - 8u),
        static_cast<std::uint32_t>(R(state, 12)));
    memory.WriteU32(Address(incoming_sp - 128u),
        static_cast<std::uint32_t>(incoming_sp));
    state.sp -= 128u;
}

void RestoreFrame(GuestMemory& memory, Registers& state)
{
    state.sp += 128u;
    R(state, 28) = ReadU64(memory, Address(state.sp - 40u));
    R(state, 29) = ReadU64(memory, Address(state.sp - 32u));
    R(state, 30) = ReadU64(memory, Address(state.sp - 24u));
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void Propagate(GuestMemory& memory, Registers& state)
{
    SaveFrame(memory, state);
    R(state, 29) = R(state, 3);
    R(state, 31) = R(state, 4);
    R(state, 30) = R(state, 5);
    state.lr = 0x830543acu;
    Classify(memory, state);
    R(state, 11) = static_cast<std::uint32_t>(R(state, 3)) & 0xffu;
    CompareS(state.cr0, R(state, 11), 0u, state.xer_so);
    if (!state.cr0.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 29) + 8u));
        Rlwimi(R(state, 11), R(state, 31), 14, 0x1c000u);
        memory.WriteU32(Address(R(state, 29) + 8u),
            static_cast<std::uint32_t>(R(state, 11)));
        RestoreFrame(memory, state);
        return;
    }
    R(state, 10) = 0u;
    CompareU(state.cr6, R(state, 31), 0u, state.xer_so);
    if (!state.cr6.eq)
    {
        R(state, 9) = 0u;
        R(state, 11) = R(state, 31);
        do
        {
            R(state, 7) = Srw(R(state, 30), R(state, 9));
            R(state, 8) = 1u;
            R(state, 7) = static_cast<std::uint32_t>(R(state, 7)) & 3u;
            state.xer_ca = static_cast<std::uint32_t>(R(state, 11)) > 0u;
            R(state, 11) -= 1u;
            CompareS(state.cr0, R(state, 11), 0u, state.xer_so);
            R(state, 9) += 2u;
            R(state, 8) = Slw(R(state, 8), R(state, 7));
            R(state, 10) = R(state, 8) | R(state, 10);
        } while (!state.cr0.eq);
    }
    R(state, 9) = static_cast<std::uint64_t>(
        std::int64_t{-1855389696});
    R(state, 7) = memory.ReadU32(Address(R(state, 29) + 8u));
    R(state, 8) = 0u;
    R(state, 9) |= 5192u;
    R(state, 8) |= 36262u;
    R(state, 11) = R(state, 10) & 0xffffffffu;
    R(state, 9) = ((R(state, 8) << 32) & 0xffffffff00000000ull) |
        (R(state, 9) & 0xffffffffu);
    R(state, 4) = WordRotateMask(R(state, 7), 18, 0x7u);
    R(state, 5) = R(state, 9);
    R(state, 8) = WordRotateMask(R(state, 7), 31, 0xfu);
    R(state, 6) = R(state, 30);
    R(state, 9) = WordRotateMask(R(state, 31), 1, 0xfffffffeu);
    R(state, 3) = 3u;
    R(state, 5) = Srd(R(state, 5), R(state, 11));
    R(state, 5) = Srd(R(state, 5), R(state, 11));
    R(state, 11) = Srd(R(state, 5), R(state, 11));
    R(state, 11) = static_cast<std::uint32_t>(R(state, 11)) & 7u;
    R(state, 11) = R(state, 4) - R(state, 11);
    R(state, 5) = R(state, 11) + R(state, 31);
    R(state, 11) = R(state, 8) - R(state, 10);
    while (true)
    {
        CompareU(state.cr6, R(state, 11), 0u, state.xer_so);
        if (state.cr6.eq) break;
        R(state, 10) = R(state, 11) - 1u;
        R(state, 4) = R(state, 11) - 1u;
        R(state, 10) = R(state, 11) & ~R(state, 10);
        R(state, 4) = R(state, 11) & ~R(state, 4);
        R(state, 10) = Cntlzw(R(state, 10));
        R(state, 11) -= R(state, 4);
        state.xer_ca = static_cast<std::uint32_t>(R(state, 10)) <= 31u;
        R(state, 10) = 31u - R(state, 10);
        R(state, 4) = Slw(R(state, 3), R(state, 9));
        R(state, 6) &= ~R(state, 4);
        R(state, 10) = Slw(R(state, 10), R(state, 9));
        R(state, 9) += 2u;
        R(state, 6) |= R(state, 10);
    }
    R(state, 4) = 0u;
    while (true)
    {
        CompareU(state.cr6, R(state, 8), 0u, state.xer_so);
        if (state.cr6.eq) break;
        R(state, 11) = R(state, 8) - 1u;
        R(state, 10) = 0u;
        R(state, 11) = R(state, 8) & ~R(state, 11);
        CompareU(state.cr6, R(state, 5), 0u, state.xer_so);
        R(state, 11) = Cntlzw(R(state, 11));
        state.xer_ca = static_cast<std::uint32_t>(R(state, 11)) <= 31u;
        R(state, 9) = 31u - R(state, 11);
        if (!state.cr6.eq)
        {
            R(state, 11) = 0u;
            while (true)
            {
                R(state, 31) = Srw(R(state, 6), R(state, 11));
                R(state, 31) = static_cast<std::uint32_t>(R(state, 31)) & 3u;
                CompareS(state.cr6, R(state, 9), R(state, 31),
                    state.xer_so);
                if (state.cr6.eq)
                {
                    R(state, 11) = WordRotateMask(R(state, 9), 1,
                        0xfffffffeu);
                    R(state, 9) = Slw(R(state, 3), R(state, 11));
                    R(state, 11) = Slw(R(state, 10), R(state, 11));
                    R(state, 10) = R(state, 4) & ~R(state, 9);
                    R(state, 4) = R(state, 10) | R(state, 11);
                    break;
                }
                R(state, 10) += 1u;
                R(state, 11) += 2u;
                CompareWordsU(state.cr6, R(state, 10), R(state, 5),
                    state.xer_so);
                if (!state.cr6.lt) break;
            }
        }
        R(state, 11) = R(state, 8) - 1u;
        R(state, 11) = R(state, 8) & ~R(state, 11);
        R(state, 8) -= R(state, 11);
    }
    R(state, 5) = static_cast<std::uint32_t>(R(state, 5)) & 7u;
    R(state, 11) = WordRotateMask(R(state, 7), 0,
        0xfffffffffffe3fffull);
    R(state, 8) = WordRotateMask(R(state, 5), 14, 0xffffc000u);
    R(state, 9) = 0u;
    R(state, 11) |= R(state, 8);
    R(state, 10) = R(state, 29) + 40u;
    memory.WriteU32(Address(R(state, 29) + 8u),
        static_cast<std::uint32_t>(R(state, 11)));
    while (true)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 29) + 8u));
        R(state, 11) = WordRotateMask(R(state, 11), 13, 7u);
        CompareWordsU(state.cr6, R(state, 9), R(state, 11), state.xer_so);
        if (!state.cr6.lt) break;
        R(state, 8) = memory.ReadU32(Address(R(state, 10)));
        R(state, 11) = WordRotateMask(R(state, 5), 25, 0xfe000000u);
        R(state, 3) = WordRotateMask(R(state, 6), 27, 6u);
        R(state, 31) = WordRotateMask(R(state, 6), 29, 6u);
        R(state, 30) = WordRotateMask(R(state, 6), 31, 6u);
        R(state, 28) = WordRotateMask(R(state, 6), 1, 6u);
        R(state, 7) = memory.ReadU32(Address(R(state, 8)));
        R(state, 9) += 1u;
        R(state, 10) += 4u;
        R(state, 7) = WordRotateMask(R(state, 7), 0,
            0xfffffffff1ffffffull);
        R(state, 7) |= R(state, 11);
        R(state, 11) = WordRotateMask(R(state, 7), 27, 0xffu);
        R(state, 7) = WordRotateMask(R(state, 7), 0,
            0xffffffffffffe01full);
        R(state, 3) = Srw(R(state, 11), R(state, 3));
        R(state, 31) = Srw(R(state, 11), R(state, 31));
        R(state, 30) = Srw(R(state, 11), R(state, 30));
        Rlwimi(R(state, 31), R(state, 3), 2, 0xcu);
        R(state, 11) = Srw(R(state, 11), R(state, 28));
        R(state, 3) = static_cast<std::uint32_t>(R(state, 31)) & 0xfu;
        Rlwimi(R(state, 30), R(state, 3), 2, 0xfffffffcu);
        Rlwimi(R(state, 11), R(state, 30), 2, 0xfffffffcu);
        R(state, 11) = WordRotateMask(R(state, 11), 5, 0xffffffe0u);
        R(state, 11) |= R(state, 7);
        memory.WriteU32(Address(R(state, 8)),
            static_cast<std::uint32_t>(R(state, 11)));
    }
    R(state, 10) = memory.ReadU32(Address(R(state, 29) + 4u));
    while (true)
    {
        CompareU(state.cr6, R(state, 10), 0u, state.xer_so);
        if (state.cr6.eq) break;
        R(state, 11) = memory.ReadU32(Address(R(state, 10) + 16u));
        CompareU(state.cr6, R(state, 11), 0u, state.xer_so);
        if (!state.cr6.eq)
        {
            R(state, 9) = memory.ReadU32(Address(R(state, 10)));
            R(state, 11) = WordRotateMask(R(state, 9), 0,
                0x0e000000u);
            CompareS(state.cr0, R(state, 11), 0u, state.xer_so);
            if (!state.cr0.eq)
            {
                R(state, 11) = WordRotateMask(R(state, 9), 27, 0xffu);
                R(state, 9) = WordRotateMask(R(state, 9), 0,
                    0xffffffffffffe01full);
                R(state, 8) = WordRotateMask(R(state, 11), 27, 6u);
                R(state, 7) = WordRotateMask(R(state, 11), 29, 6u);
                R(state, 6) = WordRotateMask(R(state, 11), 31, 6u);
                R(state, 11) = WordRotateMask(R(state, 11), 1, 6u);
                R(state, 8) = Srw(R(state, 4), R(state, 8));
                R(state, 7) = Srw(R(state, 4), R(state, 7));
                R(state, 6) = Srw(R(state, 4), R(state, 6));
                Rlwimi(R(state, 7), R(state, 8), 2, 0xcu);
                R(state, 11) = Srw(R(state, 4), R(state, 11));
                R(state, 8) = static_cast<std::uint32_t>(R(state, 7)) & 0xfu;
                Rlwimi(R(state, 6), R(state, 8), 2, 0xfffffffcu);
                Rlwimi(R(state, 11), R(state, 6), 2, 0xfffffffcu);
                R(state, 11) = WordRotateMask(R(state, 11), 5,
                    0xffffffe0u);
                R(state, 11) |= R(state, 9);
                memory.WriteU32(Address(R(state, 10)),
                    static_cast<std::uint32_t>(R(state, 11)));
            }
        }
        R(state, 10) = memory.ReadU32(Address(R(state, 10) + 8u));
    }
    RestoreFrame(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    switch (entry)
    {
    case 0x83053308u: Classify(memory, registers); return true;
    case 0x83054390u: Propagate(memory, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_bit_routes
