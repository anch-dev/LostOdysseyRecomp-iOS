#include "lo_semantics/legacy_descriptor_layout_routes.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_layout_routes
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

void CompareU(Registers& state, std::uint64_t left, std::uint64_t right,
    Condition& cr)
{
    const auto l = static_cast<std::uint32_t>(left);
    const auto r = static_cast<std::uint32_t>(right);
    cr = {std::uint8_t(l < r), std::uint8_t(l > r), std::uint8_t(l == r),
        state.xer_so};
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

std::uint64_t Cntlzw(std::uint64_t value)
{ return std::countl_zero(static_cast<std::uint32_t>(value)); }

void Classify(GuestMemory& memory, Registers& state)
{
    R(state, 11) = memory.ReadU32(Address(R(state, 3) + 8u));
    R(state, 3) = 0u;
    R(state, 11) = WordRotateMask(R(state, 11), 25, 0x7fu);
    CompareU(state, R(state, 11), 1u, state.cr6);
    if (!state.cr6.lt)
    {
        CompareU(state, R(state, 11), 30u, state.cr6);
        R(state, 10) = state.cr6.gt ? 0u : 1u;
    }
    else R(state, 10) = 0u;
    R(state, 10) = static_cast<std::uint32_t>(R(state, 10)) & 0xffu;
    CompareS(state, R(state, 10), state.cr0);
    if (state.cr0.eq) return;
    CompareU(state, R(state, 11), 16u, state.cr6);
    if (!state.cr6.lt)
    {
        CompareU(state, R(state, 11), 24u, state.cr6);
        if (!state.cr6.gt) return;
        CompareU(state, R(state, 11), 29u, state.cr6);
        if (state.cr6.eq) return;
    }
    R(state, 3) = 1u;
}

void SelectLayout(GuestMemory& memory, Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u),
        static_cast<std::uint32_t>(R(state, 12)));
    WriteU64(memory, Address(state.sp - 16u), R(state, 31));
    memory.WriteU32(Address(state.sp - 96u),
        static_cast<std::uint32_t>(state.sp));
    state.sp -= 96u;
    R(state, 31) = R(state, 3);
    state.lr = 0x82fb6858u;
    Classify(memory, state);
    R(state, 11) = static_cast<std::uint32_t>(R(state, 3)) & 0xffu;
    CompareS(state, R(state, 11), state.cr0);
    if (state.cr0.eq) R(state, 3) = 0u;
    else
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 31) + 8u));
        R(state, 11) = WordRotateMask(R(state, 11), 25, 0x7fu);
        R(state, 11) += static_cast<std::uint64_t>(-25);
        state.xer_ca = static_cast<std::uint32_t>(R(state, 11)) <= 3u;
        R(state, 11) = 3u - R(state, 11);
        R(state, 11) = state.xer_ca ? 0u : UINT64_MAX;
        R(state, 3) = static_cast<std::uint32_t>(R(state, 11)) & 1u;
    }
    state.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void SaveFrame(GuestMemory& memory, Registers& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    state.lr = 0x83054650u;
    for (unsigned index = 25u; index <= 31u; ++index)
        WriteU64(memory, Address(incoming_sp - 64u + (index - 25u) * 8u),
            R(state, index));
    memory.WriteU32(Address(incoming_sp - 8u),
        static_cast<std::uint32_t>(R(state, 12)));
    memory.WriteU32(Address(incoming_sp - 144u),
        static_cast<std::uint32_t>(incoming_sp));
    state.sp -= 144u;
}

void RestoreFrame(GuestMemory& memory, Registers& state)
{
    state.sp += 144u;
    for (unsigned index = 25u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 64u + (index - 25u) * 8u));
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
}

void Layout(GuestMemory& memory, Registers& state)
{
    SaveFrame(memory, state);
    R(state, 11) = R(state, 4) + 10u;
    R(state, 30) = R(state, 3);
    R(state, 11) = WordRotateMask(R(state, 11), 2, 0xfffffffcu);
    R(state, 25) = R(state, 5);
    R(state, 26) = R(state, 7);
    R(state, 27) = memory.ReadU32(Address(R(state, 11) + R(state, 30)));
    R(state, 11) = memory.ReadU32(Address(R(state, 27)));
    R(state, 11) = WordRotateMask(R(state, 11), 30, 1u);
    CompareS(state, R(state, 11), state.cr0);
    if (!state.cr0.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 26)));
        R(state, 11) |= 1u;
        memory.WriteU32(Address(R(state, 26)),
            static_cast<std::uint32_t>(R(state, 11)));
    }
    R(state, 28) = memory.ReadU32(Address(R(state, 27)));
    R(state, 3) = R(state, 30);
    R(state, 31) = 0u;
    R(state, 29) = WordRotateMask(R(state, 28), 7, 7u);
    state.lr = 0x83054698u;
    SelectLayout(memory, state);
    R(state, 11) = static_cast<std::uint32_t>(R(state, 3)) & 0xffu;
    CompareS(state, R(state, 11), state.cr0);
    if (!state.cr0.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 30) + 8u));
        R(state, 6) = WordRotateMask(R(state, 11), 31, 0xfu);
        R(state, 11) = R(state, 6);
        CompareS(state, R(state, 11), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 8) = 0u;
            R(state, 10) = 0u;
            while (true)
            {
                CompareU(state, R(state, 8), R(state, 29), state.cr6);
                if (!state.cr6.lt) break;
                R(state, 7) = WordRotateMask(R(state, 28), 27, 0xffu);
                R(state, 9) = R(state, 11) - 1u;
                R(state, 8) += 1u;
                R(state, 9) = R(state, 11) & ~R(state, 9);
                R(state, 11) -= R(state, 9);
                CompareS(state, R(state, 11), state.cr0);
                R(state, 9) = Cntlzw(R(state, 9));
                state.xer_ca = static_cast<std::uint32_t>(R(state, 9)) <= 31u;
                R(state, 9) = 31u - R(state, 9);
                R(state, 7) = Srw(R(state, 7), R(state, 10));
                R(state, 10) += 2u;
                R(state, 7) -= R(state, 9);
                R(state, 9) = WordRotateMask(R(state, 9), 1, 0xfffffffeu);
                R(state, 7) = static_cast<std::uint32_t>(R(state, 7)) & 3u;
                R(state, 9) = Slw(R(state, 7), R(state, 9));
                R(state, 31) = R(state, 9) | R(state, 31);
                if (state.cr0.eq) break;
            }
        }
        if (R(state, 6) != 0u)
        {
        R(state, 11) = ~R(state, 6);
        R(state, 10) = Cntlzw(R(state, 6));
        R(state, 9) = static_cast<std::uint32_t>(R(state, 11)) & 0xfu;
        state.xer_ca = static_cast<std::uint32_t>(R(state, 10)) <= 31u;
        R(state, 7) = 31u - R(state, 10);
        while (true)
        {
            CompareU(state, R(state, 9), 0u, state.cr6);
            if (state.cr6.eq) break;
            R(state, 11) = R(state, 9) - 1u;
            R(state, 11) = R(state, 9) & ~R(state, 11);
            R(state, 11) = Cntlzw(R(state, 11));
            state.xer_ca = static_cast<std::uint32_t>(R(state, 11)) <= 31u;
            R(state, 11) = 31u - R(state, 11);
            CompareU(state, R(state, 11), R(state, 7), state.cr6);
            if (state.cr6.gt)
            {
                R(state, 10) = WordRotateMask(R(state, 7), 1, 0xfffffffeu);
                R(state, 10) = Srw(R(state, 31), R(state, 10));
                R(state, 10) += R(state, 7);
            }
            else
            {
                R(state, 10) = Srw(R(state, 6), R(state, 11));
                R(state, 10) = Slw(R(state, 10), R(state, 11));
                R(state, 8) = R(state, 10) - 1u;
                R(state, 10) &= ~R(state, 8);
                R(state, 10) = Cntlzw(R(state, 10));
                state.xer_ca = static_cast<std::uint32_t>(R(state, 10)) <= 31u;
                R(state, 10) = 31u - R(state, 10);
                R(state, 8) = WordRotateMask(R(state, 10), 1, 0xfffffffeu);
                R(state, 8) = Srw(R(state, 31), R(state, 8));
                R(state, 10) = R(state, 8) + R(state, 10);
            }
            R(state, 10) = static_cast<std::uint32_t>(R(state, 10)) & 3u;
            R(state, 8) = R(state, 9) - 1u;
            R(state, 10) -= R(state, 11);
            R(state, 11) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
            R(state, 10) = static_cast<std::uint32_t>(R(state, 10)) & 3u;
            R(state, 8) = R(state, 9) & ~R(state, 8);
            R(state, 9) -= R(state, 8);
            R(state, 11) = Slw(R(state, 10), R(state, 11));
            R(state, 31) = R(state, 11) | R(state, 31);
        }
        }
    }
    else
    {
        R(state, 10) = 0u;
        CompareU(state, R(state, 29), 0u, state.cr6);
        if (!state.cr6.eq)
        {
            R(state, 9) = WordRotateMask(R(state, 28), 27, 0xffu);
            R(state, 11) = 0u;
            do
            {
                R(state, 8) = Srw(R(state, 9), R(state, 11));
                R(state, 8) -= R(state, 10);
                R(state, 10) += 1u;
                R(state, 8) = static_cast<std::uint32_t>(R(state, 8)) & 3u;
                CompareU(state, R(state, 10), R(state, 29), state.cr6);
                R(state, 8) = Slw(R(state, 8), R(state, 11));
                R(state, 11) += 2u;
                R(state, 31) = R(state, 8) | R(state, 31);
            } while (state.cr6.lt);
        }
        R(state, 10) = R(state, 29);
        CompareU(state, R(state, 29), 4u, state.cr6);
        if (state.cr6.lt)
        {
            R(state, 11) = R(state, 29) - 1u;
            R(state, 9) = WordRotateMask(R(state, 28), 27, 0xffu);
            R(state, 8) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
            R(state, 11) = WordRotateMask(R(state, 29), 1, 0xfffffffeu);
            R(state, 9) = Srw(R(state, 9), R(state, 8));
            do
            {
                R(state, 8) = R(state, 9) - R(state, 10);
                R(state, 10) += 1u;
                R(state, 8) = static_cast<std::uint32_t>(R(state, 8)) & 3u;
                R(state, 8) = Slw(R(state, 8), R(state, 11));
                R(state, 11) += 2u;
                R(state, 31) = R(state, 8) | R(state, 31);
                CompareU(state, R(state, 11), 8u, state.cr6);
            } while (state.cr6.lt);
        }
    }
    R(state, 11) = memory.ReadU32(Address(R(state, 26)));
    Rlwimi(R(state, 11), R(state, 31), 1, 0x1feu);
    memory.WriteU32(Address(R(state, 26)),
        static_cast<std::uint32_t>(R(state, 11)));
    R(state, 10) = memory.ReadU32(Address(R(state, 27) + 12u));
    R(state, 10) = memory.ReadU32(Address(R(state, 10) + 8u));
    R(state, 10) = WordRotateMask(R(state, 10), 25, 0x7fu);
    CompareU(state, R(state, 10), 124u, state.cr6);
    if (state.cr6.eq) R(state, 10) = 1u;
    else
    {
        CompareU(state, R(state, 10), 123u, state.cr6);
        R(state, 10) = state.cr6.eq ? 1u : 0u;
    }
    R(state, 10) = static_cast<std::uint32_t>(R(state, 10)) & 0xffu;
    CompareS(state, R(state, 10), state.cr0);
    if (!state.cr0.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 27)));
        R(state, 11) = WordRotateMask(R(state, 11), 31, 1u);
        CompareS(state, R(state, 11), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 11) = memory.ReadU32(Address(R(state, 25)));
            R(state, 11) |= 128u;
            memory.WriteU32(Address(R(state, 25)),
                static_cast<std::uint32_t>(R(state, 11)));
        }
        R(state, 11) = memory.ReadU32(Address(R(state, 27)));
        R(state, 11) = WordRotateMask(R(state, 11), 0, 0x18u);
        CompareS(state, R(state, 11), state.cr0);
        if (!state.cr0.eq)
        {
            R(state, 11) = memory.ReadU32(Address(R(state, 26)));
            R(state, 11) |= 512u;
            memory.WriteU32(Address(R(state, 26)),
                static_cast<std::uint32_t>(R(state, 11)));
            R(state, 11) = memory.ReadU32(Address(R(state, 27)));
            R(state, 11) = WordRotateMask(R(state, 11), 0, 8u);
            CompareS(state, R(state, 11), state.cr0);
            if (!state.cr0.eq)
            {
                R(state, 11) = memory.ReadU32(Address(R(state, 25) + 4u));
                R(state, 11) |= 536870912u;
                memory.WriteU32(Address(R(state, 25) + 4u),
                    static_cast<std::uint32_t>(R(state, 11)));
            }
        }
        R(state, 11) = memory.ReadU32(Address(R(state, 27)));
        R(state, 3) = 1u;
        R(state, 10) = memory.ReadU32(Address(R(state, 26)));
        Rlwimi(R(state, 10), R(state, 11), 26, 0x7f800u);
        memory.WriteU32(Address(R(state, 26)),
            static_cast<std::uint32_t>(R(state, 10)));
    }
    else
    {
        // At this point r11 still holds the final packed output word.
        R(state, 11) |= 1024u;
        memory.WriteU32(Address(R(state, 26)),
            static_cast<std::uint32_t>(R(state, 11)));
        R(state, 10) = memory.ReadU32(Address(R(state, 27)));
        R(state, 9) = WordRotateMask(R(state, 10), 15, 0xffu);
        R(state, 8) = WordRotateMask(R(state, 10), 31, 1u);
        CompareS(state, R(state, 8), state.cr0);
        if (!state.cr0.eq) R(state, 9) |= 128u;
        R(state, 10) = WordRotateMask(R(state, 10), 0, 0x10u);
        CompareS(state, R(state, 10), state.cr0);
        if (!state.cr0.eq) R(state, 9) |= 64u;
        Rlwimi(R(state, 11), R(state, 9), 11, 0x7f800u);
        R(state, 3) = 0u;
        memory.WriteU32(Address(R(state, 26)),
            static_cast<std::uint32_t>(R(state, 11)));
    }
    RestoreFrame(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Registers& registers)
{
    switch (entry)
    {
    case 0x82fb67e8u: Classify(memory, registers); return true;
    case 0x82fb6840u: SelectLayout(memory, registers); return true;
    case 0x83054648u: Layout(memory, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_layout_routes
