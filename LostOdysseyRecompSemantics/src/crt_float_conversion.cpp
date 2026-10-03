#include "lo_semantics/crt_float_conversion.h"

#include "lo_semantics/memory_move.h"
#include "lo_semantics/recovery_abi.h"

#include <cstdint>

namespace lo::semantic::gpu::crt_float_conversion
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }
std::uint32_t W(std::uint64_t value) { return static_cast<std::uint32_t>(value); }
std::int32_t SW(std::uint64_t value) { return static_cast<std::int32_t>(value); }
std::uint16_t H(std::uint64_t value) { return static_cast<std::uint16_t>(value); }

void CompareUnsigned(Condition& cr, std::uint64_t value,
    std::uint32_t other, std::uint8_t so)
{
    const auto word = W(value);
    cr = {std::uint8_t(word < other), std::uint8_t(word > other),
        std::uint8_t(word == other), so};
}

void CompareSigned(Condition& cr, std::uint64_t value,
    std::int32_t other, std::uint8_t so)
{
    const auto word = SW(value);
    cr = {std::uint8_t(word < other), std::uint8_t(word > other),
        std::uint8_t(word == other), so};
}

void CompareWords(Condition& cr, std::uint64_t left,
    std::uint64_t right, std::uint8_t so)
{
    CompareUnsigned(cr, left, W(right), so);
}

void AddImmediate(Registers& state, unsigned index, std::int32_t increment)
{
    const auto before = R(state, index);
    const auto after = before + static_cast<std::uint64_t>(increment);
    state.xer_ca = std::uint8_t(increment < 0 ?
        W(before) >= static_cast<std::uint32_t>(-increment) :
        W(before) > 0xffffffffu - static_cast<std::uint32_t>(increment));
    R(state, index) = after;
    CompareSigned(state.cr0, after, 0, state.xer_so);
}

void SaveFrame(GuestMemory& memory, Registers& state,
    unsigned first_saved, std::uint32_t frame_size)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(state.sp - 8u), Address(R(state, 12)));
    for (unsigned index = first_saved; index <= 31; ++index)
        WriteU64(memory, Address(state.sp - 16u - 8u * (31u - index)),
            R(state, index));
    memory.WriteU32(Address(state.sp - frame_size), Address(state.sp));
    state.sp -= frame_size;
}

void RestoreFrame(GuestMemory& memory, Registers& state,
    unsigned first_saved, std::uint32_t frame_size)
{
    state.sp += frame_size;
    R(state, 12) = memory.ReadU32(Address(state.sp - 8u));
    state.lr = R(state, 12);
    for (unsigned index = first_saved; index <= 31; ++index)
        R(state, index) = ReadU64(memory,
            Address(state.sp - 16u - 8u * (31u - index)));
}

void CallCopy(GuestMemory& memory, Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    R(state, 3) = CopyGuestMemory(memory, R(state, 3), Address(R(state, 4)),
        R(state, 5), Address(state.sp));
}

void CallFatal(GuestMemory& memory, Dependencies dependencies,
    Registers& state, GuestAddress return_address)
{
    state.lr = return_address;
    (void)crt_float_environment::Apply(0x82b7ff08u, memory,
        dependencies.environment, state);
}

// The factor tables hold 12-byte extended powers of ten. Multiply their five
// significant 16-bit limbs into the live 96-bit workspace. The source and
// destination accesses are deliberately ordered: callers can alias the table
// with the stack, and the PPC propagates each word carry immediately.
void MultiplyFactor(GuestMemory& memory, Registers& state,
    GuestAddress factor, GuestAddress result, bool second_pass)
{
    const auto sp = Address(state.sp);
    const auto source_end = sp + 98u;
    const auto result_end = result + 6u;
    R(state, 31) = 0;
    R(state, 8) = result_end;
    if (second_pass)
        R(state, 4) = 5;
    else
        R(state, 3) = 5;
    do
    {
        if (second_pass)
            R(state, 3) = R(state, 4);
        R(state, 11) = WordRotateMask(R(state, 31), 1, 0xfffffffeu);
        R(state, 10) = source_end;
        R(state, 5) = factor + 2u;
        R(state, 6) = R(state, 10) - R(state, 11);
        if (!second_pass)
            state.ctr = W(R(state, 3));
        const auto limbs = W(second_pass ? R(state, 4) : R(state, 3));
        for (unsigned limb = 0; limb < limbs; ++limb)
        {
            R(state, 10) = memory.ReadU16(Address(R(state, 5)));
            R(state, 7) = 0;
            R(state, 9) = memory.ReadU16(Address(R(state, 6)));
            R(state, 11) = memory.ReadU32(Address(R(state, 8)) + 2u);
            R(state, 9) = W(R(state, 10) * R(state, 9));
            R(state, 10) = R(state, 11) + R(state, 9);
            CompareWords(state.cr6, R(state, 10), R(state, 11), state.xer_so);
            if (!state.cr6.lt)
                CompareWords(state.cr6, R(state, 10), R(state, 9), state.xer_so);
            if (state.cr6.lt)
                R(state, 7) = 1;
            CompareSigned(state.cr6, R(state, 7), 0, state.xer_so);
            memory.WriteU32(Address(R(state, 8)) + 2u, W(R(state, 10)));
            if (!state.cr6.eq)
            {
                R(state, 11) = memory.ReadU16(Address(R(state, 8)));
                R(state, 11) += 1u;
                memory.WriteU16(Address(R(state, 8)), H(R(state, 11)));
            }
            if (second_pass)
                AddImmediate(state, 3, -1);
            R(state, 6) -= 2u;
            R(state, 5) += 2u;
            if (!second_pass)
                --state.ctr;
        }
        if (second_pass)
        {
            AddImmediate(state, 4, -1);
            R(state, 8) -= 2u;
            R(state, 31) += 1u;
        }
        else
        {
            AddImmediate(state, 3, -1);
            R(state, 8) -= 2u;
            R(state, 31) += 1u;
        }
    } while (state.cr0.gt);
}

void ShiftProductLeft(GuestMemory& memory, Registers& state,
    GuestAddress high, GuestAddress middle, GuestAddress low)
{
    const auto low_word = memory.ReadU32(low);
    const auto middle_word = memory.ReadU32(middle);
    const auto high_word = memory.ReadU32(high);
    R(state, 9) = WordRotateMask(low_word, 1, 1u);
    R(state, 8) = WordRotateMask(middle_word, 1, 1u);
    R(state, 7) = WordRotateMask(middle_word, 1, 0xfffffffeu);
    R(state, 6) = WordRotateMask(high_word, 1, 0xfffffffeu);
    R(state, 10) = WordRotateMask(low_word, 1, 0xfffffffeu);
    R(state, 9) = R(state, 7) | R(state, 9);
    memory.WriteU32(middle, W(R(state, 9)));
    R(state, 9) = R(state, 6) | R(state, 8);
    memory.WriteU32(low, W(R(state, 10)));
    memory.WriteU32(high, W(R(state, 9)));
}

// Both decimal scaling passes use the same 80-bit normalization rule. The
// final halfword is the discarded part used by the original tie test.
void NormalizeProduct(GuestMemory& memory, Registers& state,
    GuestAddress high, GuestAddress middle, GuestAddress low,
    GuestAddress discarded, std::uint16_t exponent)
{
    auto adjusted = H(exponent + W(R(state, 20)));
    R(state, 11) = adjusted;
    R(state, 10) = memory.ReadU32(low);
    CompareSigned(state.cr0, static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int16_t>(adjusted))),
        0, state.xer_so);
    while (state.cr0.gt)
    {
        R(state, 6) = memory.ReadU32(high);
        R(state, 9) = WordRotateMask(R(state, 6), 0, 0x80000000u);
        CompareSigned(state.cr0, R(state, 9), 0, state.xer_so);
        if (!state.cr0.eq)
            break;
        adjusted = H(adjusted + W(R(state, 21)));
        R(state, 11) = adjusted;
        ShiftProductLeft(memory, state, high, middle, low);
        CompareSigned(state.cr0, static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int16_t>(adjusted))),
            0, state.xer_so);
    }
    CompareSigned(state.cr0, static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int16_t>(adjusted))),
        0, state.xer_so);
    if (!state.cr0.gt)
    {
        adjusted = H(adjusted + W(R(state, 21)));
        R(state, 11) = adjusted;
        CompareSigned(state.cr0, static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int16_t>(adjusted))),
            0, state.xer_so);
        if (state.cr0.lt)
        {
            R(state, 6) = memory.ReadU32(high);
            R(state, 7) = memory.ReadU32(middle);
            do
            {
                R(state, 9) = memory.ReadU16(discarded);
                R(state, 9) = W(R(state, 9)) & 1u;
                CompareSigned(state.cr0, R(state, 9), 0, state.xer_so);
                if (!state.cr0.eq)
                    R(state, 27) += 1u;
                R(state, 8) = (W(R(state, 6)) & 1u) ? R(state, 14) : 0u;
                R(state, 9) = (W(R(state, 7)) & 1u) ? R(state, 14) : 0u;
                R(state, 10) = WordRotateMask(R(state, 10), 31, 0x7fffffffu) | R(state, 9);
                R(state, 7) = WordRotateMask(R(state, 7), 31, 0x7fffffffu) | R(state, 8);
                R(state, 6) = WordRotateMask(R(state, 6), 31, 0x7fffffffu);
                memory.WriteU32(low, W(R(state, 10)));
                adjusted = H(adjusted + 1u);
                R(state, 11) = adjusted;
                CompareSigned(state.cr0, static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(static_cast<std::int16_t>(adjusted))),
                    0, state.xer_so);
            } while (state.cr0.lt);
            memory.WriteU32(middle, W(R(state, 7)));
            CompareSigned(state.cr6, R(state, 27), 0, state.xer_so);
            memory.WriteU32(high, W(R(state, 6)));
            if (!state.cr6.eq)
            {
                R(state, 10) = memory.ReadU16(discarded);
                R(state, 10) |= 1u;
                memory.WriteU16(discarded, H(R(state, 10)));
                R(state, 10) = memory.ReadU32(low);
            }
        }
    }

    R(state, 9) = memory.ReadU16(discarded);
    CompareUnsigned(state.cr6, R(state, 9), 0x8000u, state.xer_so);
    bool round_up = state.cr6.gt;
    if (!round_up)
    {
        R(state, 10) = W(R(state, 10)) & 0x1ffffu;
        CompareWords(state.cr6, R(state, 10), R(state, 17), state.xer_so);
        round_up = state.cr6.eq;
    }
    if (round_up)
    {
        R(state, 10) = memory.ReadU32(low - 2u);
        CompareSigned(state.cr6, R(state, 10), -1, state.xer_so);
        if (state.cr6.eq)
        {
            R(state, 10) = memory.ReadU32(middle - 2u);
            memory.WriteU32(low - 2u, 0);
            CompareSigned(state.cr6, R(state, 10), -1, state.xer_so);
            if (state.cr6.eq)
            {
                R(state, 10) = memory.ReadU16(high);
                memory.WriteU32(middle - 2u, 0);
                CompareUnsigned(state.cr6, R(state, 10), 0xffffu, state.xer_so);
                if (state.cr6.eq)
                {
                    memory.WriteU16(high, 0x8000u);
                    adjusted = H(adjusted + 1u);
                    R(state, 11) = adjusted;
                }
                else
                {
                    R(state, 10) += 1u;
                    memory.WriteU16(high, H(R(state, 10)));
                }
            }
            else
            {
                R(state, 10) += 1u;
                memory.WriteU32(middle - 2u, W(R(state, 10)));
            }
        }
        else
        {
            R(state, 10) += 1u;
            memory.WriteU32(low - 2u, W(R(state, 10)));
        }
    }
    R(state, 11) = adjusted;
    CompareUnsigned(state.cr6, R(state, 11), 0x7fffu, state.xer_so);
}

void StoreScaledResult(GuestMemory& memory, Registers& state,
    GuestAddress product, bool second_pass)
{
    const auto sp = Address(state.sp);
    const auto exponent = H(R(state, 11));
    if (!state.cr6.lt)
    {
        R(state, 11) = W(R(state, 26)) ?
            R(state, 15) : R(state, 16);
        memory.WriteU32(sp + 88u, W(R(state, 11)));
        R(state, 31) = memory.ReadU32(sp + 372u);
        memory.WriteU32(sp + 92u, 0);
        memory.WriteU32(sp + 96u, 0);
        R(state, 29) = 0;
        R(state, 30) = 0;
        return;
    }
    R(state, 10) = W(R(state, 26));
    R(state, 9) = memory.ReadU16(product + 8u);
    R(state, 31) = memory.ReadU32(sp + 372u);
    R(state, 11) = W(R(state, 10)) | exponent;
    R(state, 10) = memory.ReadU32(product + 4u);
    memory.WriteU16(sp + 98u, H(R(state, 9)));
    memory.WriteU32(sp + 94u, W(R(state, 10)));
    R(state, 10) = memory.ReadU32(product);
    R(state, 29) = memory.ReadU32(sp + 96u);
    memory.WriteU16(sp + 88u, H(R(state, 11)));
    memory.WriteU32(sp + 90u, W(R(state, 10)));
    R(state, 30) = memory.ReadU32(sp + 92u);
    (void)second_pass;
}

void MultiplyAndNormalize(GuestMemory& memory, Registers& state,
    GuestAddress factor, bool second_pass)
{
    const auto sp = Address(state.sp);
    const auto product = sp + (second_pass ? 120u : 104u);
    const auto discarded = sp + (second_pass ? 130u : 114u);
    R(state, 27) = 0;
    memory.WriteU32(product + 8u, 0);
    memory.WriteU32(product + 4u, 0);
    memory.WriteU32(product, 0);
    R(state, 10) = memory.ReadU16(factor);
    R(state, 11) = memory.ReadU16(sp + 88u);
    R(state, 8) = R(state, 10);
    R(state, 10) = R(state, 11) ^ R(state, 10);
    R(state, 11) = W(R(state, 11)) & 0x7fffu;
    R(state, 26) = W(R(state, 10)) & 0x8000u;
    R(state, 10) = W(R(state, 8)) & 0x7fffu;
    CompareUnsigned(state.cr6, R(state, 11), 0x7fffu, state.xer_so);
    R(state, 9) = R(state, 11) + R(state, 10);
    R(state, 28) = H(R(state, 9));
    if (!state.cr6.lt)
        goto exceptional_exponent;
    CompareUnsigned(state.cr6, R(state, 10), 0x7fffu, state.xer_so);
    if (!state.cr6.lt)
        goto exceptional_exponent;
    R(state, 9) = H(R(state, 28));
    CompareUnsigned(state.cr6, R(state, 9), 49149u, state.xer_so);
    if (state.cr6.gt)
        goto exceptional_exponent;
    CompareUnsigned(state.cr6, R(state, 9), 16319u, state.xer_so);
    if (!state.cr6.gt)
    {
        memory.WriteU32(sp + 88u, 0);
        goto zero_significand;
    }
    CompareUnsigned(state.cr6, R(state, 11), 0, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 11) = H(R(state, 9)) + 1u;
        R(state, 9) = memory.ReadU32(sp + 88u);
        R(state, 28) = H(R(state, 11));
        R(state, 9) = W(R(state, 9)) & 1u;
        CompareSigned(state.cr0, R(state, 9), 0, state.xer_so);
        if (state.cr0.eq)
        {
            CompareUnsigned(state.cr6, R(state, 30), 0, state.xer_so);
            if (state.cr6.eq)
                CompareUnsigned(state.cr6, R(state, 29), 0, state.xer_so);
            if (state.cr6.eq)
            {
                memory.WriteU16(sp + 88u, 0);
                return;
            }
        }
    }
    CompareUnsigned(state.cr6, R(state, 10), 0, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 10) = memory.ReadU32(factor);
        R(state, 11) = H(R(state, 28)) + 1u;
        R(state, 28) = H(R(state, 11));
        R(state, 10) = W(R(state, 10)) & 1u;
        CompareSigned(state.cr0, R(state, 10), 0, state.xer_so);
        if (state.cr0.eq)
        {
            R(state, 11) = memory.ReadU32(factor + 4u);
            CompareUnsigned(state.cr6, R(state, 11), 0, state.xer_so);
            if (state.cr6.eq)
            {
                R(state, 11) = memory.ReadU32(factor + 8u);
                CompareUnsigned(state.cr6, R(state, 11), 0, state.xer_so);
                if (state.cr6.eq)
                {
                    memory.WriteU32(sp + 88u, 0);
                    goto zero_significand;
                }
            }
        }
    }
    MultiplyFactor(memory, state, factor, product, second_pass);
    NormalizeProduct(memory, state, product, product + 4u,
        product + 8u, discarded, H(R(state, 28)));
    StoreScaledResult(memory, state, product, second_pass);
    return;

exceptional_exponent:
    R(state, 11) = W(R(state, 26)) ? R(state, 15) : R(state, 16);
    memory.WriteU32(sp + 88u, W(R(state, 11)));
zero_significand:
    R(state, 29) = 0;
    R(state, 30) = 0;
    memory.WriteU32(sp + 96u, 0);
    memory.WriteU32(sp + 92u, 0);
}

void SelectPowers(GuestMemory& memory, Registers& state)
{
    const auto sp = Address(state.sp);
    R(state, 7) = WordRotateMask(R(state, 11), 24, 0x00ffffffu);
    memory.WriteU32(sp + 90u, W(R(state, 10)));
    R(state, 8) = W(R(state, 11) * 19728u);
    memory.WriteU16(sp + 88u, H(R(state, 6)));
    memory.WriteU32(sp + 94u, W(R(state, 5)));
    R(state, 9) = WordRotateMask(R(state, 10), 9, 0x000001feu);
    R(state, 11) = 0xffffffff83210000ull;
    R(state, 22) = 0;
    R(state, 9) += R(state, 7);
    R(state, 11) += 23680u;
    R(state, 9) = W(R(state, 9) * 77u);
    memory.WriteU16(sp + 98u, 0);
    R(state, 23) = R(state, 11) - 96u;
    R(state, 11) = 0;
    R(state, 9) += R(state, 8);
    R(state, 15) = static_cast<std::uint64_t>(static_cast<std::int64_t>(-32768));
    R(state, 20) = 0xc002u;
    R(state, 9) += 0xffffffffecbced0cull; // -4931 * 65536 - 4852.
    R(state, 21) = 0xffffu;
    R(state, 10) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(SW(R(state, 9)) >> 16));
    R(state, 17) = 0x18000u;
    R(state, 19) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int16_t>(R(state, 10))));
    R(state, 18) = 0x8000u;
    R(state, 24) = R(state, 19);
    R(state, 16) = 0x7fff8000u;
    R(state, 25) = 0u - R(state, 24);
    CompareSigned(state.cr0, R(state, 25), 0, state.xer_so);
    if (state.cr0.lt)
    {
        R(state, 11) = 0xffffffff83210000ull;
        R(state, 25) = 0u - R(state, 25);
        R(state, 11) += 24032u;
        CompareSigned(state.cr6, R(state, 25), 0, state.xer_so);
        R(state, 23) = R(state, 11) - 96u;
    }
    else
        CompareSigned(state.cr6, R(state, 25), 0, state.xer_so);
    if (state.cr6.eq)
    {
        R(state, 29) = memory.ReadU32(sp + 96u);
        R(state, 30) = memory.ReadU32(sp + 92u);
        return;
    }
    for (;;)
    {
        R(state, 29) = memory.ReadU32(sp + 96u);
        R(state, 30) = memory.ReadU32(sp + 92u);
        R(state, 11) = W(R(state, 25)) & 7u;
        CompareSigned(state.cr0, R(state, 11), 0, state.xer_so);
        R(state, 23) += 84u;
        R(state, 25) = static_cast<std::uint64_t>(SW(R(state, 25)) >> 3);
        if (!state.cr0.eq)
        {
            R(state, 11) = W(R(state, 11) * 12u);
            R(state, 4) = R(state, 11) + R(state, 23);
            R(state, 11) = memory.ReadU16(Address(R(state, 4)) + 10u);
            CompareUnsigned(state.cr6, R(state, 11), 0x8000u, state.xer_so);
            if (!state.cr6.lt)
            {
                R(state, 3) = state.sp + 152u;
                R(state, 5) = 12;
                CallCopy(memory, state, 0x8231a710u);
                R(state, 11) = memory.ReadU32(sp + 158u);
                R(state, 4) = state.sp + 152u;
                R(state, 11) -= 1u;
                memory.WriteU32(sp + 158u, W(R(state, 11)));
            }
            MultiplyAndNormalize(memory, state, Address(R(state, 4)), false);
        }
        CompareSigned(state.cr6, R(state, 25), 0, state.xer_so);
        if (state.cr6.eq)
            break;
    }
}

void AlignMantissaForDigits(GuestMemory& memory, Registers& state)
{
    const auto sp = Address(state.sp);
    R(state, 11) = memory.ReadU16(sp + 88u);
    memory.WriteU16(sp + 88u, 0);
    R(state, 8) = R(state, 11) - 16382u;
    R(state, 31) = memory.ReadU32(sp + 88u);
    R(state, 11) = 8;
    do
    {
        R(state, 10) = WordRotateMask(R(state, 29), 1, 1u);
        R(state, 9) = WordRotateMask(R(state, 30), 1, 1u);
        R(state, 7) = WordRotateMask(R(state, 30), 1, 0xfffffffeu);
        R(state, 6) = WordRotateMask(R(state, 31), 1, 0xfffffffeu);
        AddImmediate(state, 11, -1);
        R(state, 29) = WordRotateMask(R(state, 29), 1, 0xfffffffeu);
        R(state, 30) = R(state, 7) | R(state, 10);
        R(state, 31) = R(state, 6) | R(state, 9);
    } while (!state.cr0.eq);
    memory.WriteU32(sp + 88u, W(R(state, 31)));
    CompareSigned(state.cr6, R(state, 8), 0, state.xer_so);
    memory.WriteU32(sp + 92u, W(R(state, 30)));
    memory.WriteU32(sp + 96u, W(R(state, 29)));
    if (!state.cr6.lt)
        return;
    R(state, 11) = 0u - R(state, 8);
    R(state, 11) = W(R(state, 11)) & 0xffu;
    CompareSigned(state.cr0, R(state, 11), 0, state.xer_so);
    while (state.cr0.gt)
    {
        R(state, 10) = W(R(state, 31)) & 1u;
        CompareSigned(state.cr0, R(state, 10), 0, state.xer_so);
        R(state, 9) = state.cr0.eq ? 0u : R(state, 14);
        R(state, 10) = W(R(state, 30)) & 1u;
        CompareSigned(state.cr0, R(state, 10), 0, state.xer_so);
        R(state, 10) = state.cr0.eq ? 0u : R(state, 14);
        R(state, 8) = WordRotateMask(R(state, 30), 31, 0x7fffffffu);
        R(state, 7) = WordRotateMask(R(state, 29), 31, 0x7fffffffu);
        AddImmediate(state, 11, -1);
        R(state, 31) = WordRotateMask(R(state, 31), 31, 0x7fffffffu);
        R(state, 30) = R(state, 8) | R(state, 9);
        R(state, 29) = R(state, 7) | R(state, 10);
    }
    memory.WriteU32(sp + 96u, W(R(state, 29)));
    memory.WriteU32(sp + 92u, W(R(state, 30)));
    memory.WriteU32(sp + 88u, W(R(state, 31)));
}

void ProduceDecimalDigits(GuestMemory& memory, Registers& state)
{
    const auto sp = Address(state.sp);
    R(state, 11) = memory.ReadU32(sp + 372u);
    R(state, 26) = R(state, 11) + 4u;
    R(state, 11) = memory.ReadU32(sp + 356u);
    R(state, 28) = R(state, 11);
    AddImmediate(state, 28, 1);
    R(state, 27) = R(state, 26);
    if (!state.cr0.gt)
        return;
    do
    {
        R(state, 3) = state.sp + 152u;
        R(state, 4) = state.sp + 88u;
        R(state, 5) = 12;
        CallCopy(memory, state, 0x8231af00u);
        R(state, 6) = WordRotateMask(R(state, 30), 1, 0xfffffffeu);
        R(state, 11) = WordRotateMask(R(state, 29), 1, 0xfffffffeu);
        R(state, 5) = memory.ReadU32(sp + 160u);
        R(state, 10) = WordRotateMask(R(state, 29), 1, 1u);
        R(state, 8) = WordRotateMask(R(state, 30), 1, 1u);
        R(state, 4) = WordRotateMask(R(state, 31), 1, 0xfffffffeu);
        R(state, 10) = R(state, 6) | R(state, 10);
        R(state, 7) = WordRotateMask(R(state, 11), 1, 1u);
        R(state, 11) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
        R(state, 8) = R(state, 4) | R(state, 8);
        R(state, 6) = WordRotateMask(R(state, 10), 1, 1u);
        R(state, 9) = R(state, 5) + R(state, 11);
        R(state, 10) = WordRotateMask(R(state, 10), 1, 0xfffffffeu);
        R(state, 8) = WordRotateMask(R(state, 8), 1, 0xfffffffeu);
        R(state, 10) |= R(state, 7);
        CompareWords(state.cr6, R(state, 9), R(state, 11), state.xer_so);
        R(state, 6) = R(state, 8) | R(state, 6);
        if (!state.cr6.lt)
            CompareWords(state.cr6, R(state, 9), R(state, 5), state.xer_so);
        if (state.cr6.lt)
        {
            R(state, 11) = R(state, 10) + 1u;
            R(state, 8) = 0;
            CompareWords(state.cr6, R(state, 11), R(state, 10), state.xer_so);
            if (!state.cr6.lt)
                CompareUnsigned(state.cr6, R(state, 11), 1u, state.xer_so);
            if (state.cr6.lt)
                R(state, 8) = 1;
            R(state, 10) = R(state, 11);
            CompareSigned(state.cr6, R(state, 8), 0, state.xer_so);
            if (!state.cr6.eq)
                R(state, 6) += 1u;
        }
        R(state, 8) = memory.ReadU32(sp + 156u);
        R(state, 11) = R(state, 8) + R(state, 10);
        CompareWords(state.cr6, R(state, 11), R(state, 10), state.xer_so);
        if (!state.cr6.lt)
            CompareWords(state.cr6, R(state, 11), R(state, 8), state.xer_so);
        if (state.cr6.lt)
            R(state, 6) += 1u;
        R(state, 7) = memory.ReadU32(sp + 152u);
        R(state, 10) = WordRotateMask(R(state, 11), 1, 1u);
        R(state, 8) = WordRotateMask(R(state, 9), 1, 1u);
        R(state, 7) += R(state, 6);
        R(state, 6) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
        R(state, 7) = WordRotateMask(R(state, 7), 1, 0xfffffffeu);
        R(state, 29) = WordRotateMask(R(state, 9), 1, 0xfffffffeu);
        R(state, 11) = R(state, 7) | R(state, 10);
        R(state, 30) = R(state, 6) | R(state, 8);
        AddImmediate(state, 28, -1);
        memory.WriteU32(sp + 96u, W(R(state, 29)));
        memory.WriteU32(sp + 88u, W(R(state, 11)));
        R(state, 11) = memory.ReadU8(sp + 88u);
        memory.WriteU32(sp + 92u, W(R(state, 30)));
        R(state, 11) += 48u;
        memory.WriteU8(sp + 88u, 0);
        memory.WriteU8(Address(R(state, 27)), static_cast<std::uint8_t>(R(state, 11)));
        R(state, 27) += 1u;
        if (state.cr0.gt)
            R(state, 31) = memory.ReadU32(sp + 88u);
    } while (state.cr0.gt);
}

void FinalizeDecimalRecord(GuestMemory& memory, Registers& state)
{
    const auto sp = Address(state.sp);
    R(state, 11) = R(state, 27) - 1u;
    R(state, 10) = memory.ReadU8(Address(R(state, 11)));
    R(state, 11) -= 1u;
    R(state, 10) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int8_t>(R(state, 10))));
    CompareSigned(state.cr6, R(state, 10), 53, state.xer_so);
    if (!state.cr6.lt)
    {
        for (;;)
        {
            CompareWords(state.cr6, R(state, 11), R(state, 26), state.xer_so);
            if (state.cr6.lt)
                break;
            R(state, 10) = memory.ReadU8(Address(R(state, 11)));
            CompareUnsigned(state.cr6, R(state, 10), 57, state.xer_so);
            if (!state.cr6.eq)
                break;
            R(state, 10) = 48;
            memory.WriteU8(Address(R(state, 11)), 48);
            R(state, 11) -= 1u;
        }
        R(state, 9) = memory.ReadU32(sp + 372u);
        CompareWords(state.cr6, R(state, 11), R(state, 26), state.xer_so);
        if (state.cr6.lt)
        {
            R(state, 10) = memory.ReadU16(Address(R(state, 9)));
            R(state, 11) += 1u;
            R(state, 10) += 1u;
            memory.WriteU16(Address(R(state, 9)), H(R(state, 10)));
        }
        R(state, 10) = memory.ReadU8(Address(R(state, 11)));
        R(state, 10) += 1u;
        memory.WriteU8(Address(R(state, 11)), static_cast<std::uint8_t>(R(state, 10)));
    }
    else
    {
        for (;;)
        {
            CompareWords(state.cr6, R(state, 11), R(state, 26), state.xer_so);
            if (state.cr6.lt)
                break;
            R(state, 10) = memory.ReadU8(Address(R(state, 11)));
            CompareUnsigned(state.cr6, R(state, 10), 48, state.xer_so);
            if (!state.cr6.eq)
                break;
            R(state, 11) -= 1u;
        }
        CompareWords(state.cr6, R(state, 11), R(state, 26), state.xer_so);
        if (state.cr6.lt)
        {
            R(state, 10) = memory.ReadU32(sp + 372u);
            R(state, 11) = memory.ReadU32(sp + 80u);
            CompareUnsigned(state.cr6, R(state, 11), 0x8000u, state.xer_so);
            R(state, 11) = state.cr6.eq ? 45u : 32u;
            memory.WriteU16(Address(R(state, 10)), 0);
            memory.WriteU8(Address(R(state, 10)) + 2u,
                static_cast<std::uint8_t>(R(state, 11)));
            R(state, 9) = 48;
            R(state, 11) = 1;
            memory.WriteU8(Address(R(state, 10)) + 5u, 0);
            R(state, 3) = 1;
            memory.WriteU8(Address(R(state, 26)), 48);
            memory.WriteU8(Address(R(state, 10)) + 3u, 1);
            return;
        }
    }
    R(state, 9) = memory.ReadU32(sp + 372u);
    R(state, 11) = R(state, 11) - R(state, 9);
    R(state, 3) = memory.ReadU32(sp + 84u);
    R(state, 11) -= 3u;
    R(state, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int8_t>(R(state, 11))));
    R(state, 10) = R(state, 11) + R(state, 9);
    memory.WriteU8(Address(R(state, 9)) + 3u,
        static_cast<std::uint8_t>(R(state, 11)));
    memory.WriteU8(Address(R(state, 10)) + 4u, 0);
}

void ConvertExtended(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveFrame(memory, state, 14, 320);
    const auto sp = Address(state.sp);
    WriteU64(memory, sp + 336u, R(state, 3));
    R(state, 11) = 63;
    R(state, 31) = R(state, 7);
    memory.WriteU32(sp + 364u, W(R(state, 6)));
    R(state, 8) = 1;
    WriteU64(memory, sp + 344u, R(state, 4));
    memory.WriteU32(sp + 356u, W(R(state, 5)));
    memory.WriteU8(sp + 136u, 63);
    R(state, 11) = 251;
    memory.WriteU32(sp + 372u, W(R(state, 31)));
    memory.WriteU32(sp + 84u, 1);
    memory.WriteU8(sp + 137u, 251);
    R(state, 11) = 204;
    for (unsigned offset = 138; offset <= 147; ++offset)
        memory.WriteU8(sp + offset, 204);
    R(state, 11) = 45;
    R(state, 10) = memory.ReadU16(sp + 336u);
    R(state, 6) = W(R(state, 10)) & 0x7fffu;
    R(state, 9) = WordRotateMask(R(state, 10), 0, 0xffff8000u);
    CompareSigned(state.cr0, R(state, 9), 0, state.xer_so);
    memory.WriteU32(sp + 80u, W(R(state, 9)));
    if (state.cr0.eq)
        R(state, 11) = 32;
    memory.WriteU8(Address(R(state, 31)) + 2u, static_cast<std::uint8_t>(R(state, 11)));
    R(state, 11) = W(R(state, 6)) & 0xffffu;
    CompareSigned(state.cr0, R(state, 11), 0, state.xer_so);
    R(state, 5) = memory.ReadU32(sp + 342u);
    R(state, 10) = memory.ReadU32(sp + 338u);
    if (state.cr0.eq)
    {
        CompareUnsigned(state.cr6, R(state, 10), 0, state.xer_so);
        if (state.cr6.eq)
            CompareUnsigned(state.cr6, R(state, 5), 0, state.xer_so);
        if (state.cr6.eq)
        {
            R(state, 22) = 0;
            CompareUnsigned(state.cr6, R(state, 9), 0x8000u, state.xer_so);
            R(state, 11) = state.cr6.eq ? 45u : 32u;
            memory.WriteU16(Address(R(state, 31)), 0);
            memory.WriteU8(Address(R(state, 31)) + 2u, static_cast<std::uint8_t>(R(state, 11)));
            memory.WriteU8(Address(R(state, 31)) + 3u, 1);
            R(state, 10) = 48;
            memory.WriteU8(Address(R(state, 31)) + 5u, 0);
            R(state, 3) = 1;
            memory.WriteU8(Address(R(state, 31)) + 4u, 48);
            RestoreFrame(memory, state, 14, 320);
            return;
        }
    }

    CompareUnsigned(state.cr6, R(state, 11), 0x7fffu, state.xer_so);
    R(state, 14) = 0xffffffff80000000ull;
    if (state.cr6.eq)
    {
        CompareWords(state.cr6, R(state, 10), R(state, 14), state.xer_so);
        memory.WriteU16(Address(R(state, 31)), 1);
        bool selected = false;
        bool five = false;
        if (!state.cr6.eq)
        {
            R(state, 11) = WordRotateMask(R(state, 10), 0, 0x40000000u);
            CompareSigned(state.cr0, R(state, 11), 0, state.xer_so);
            if (state.cr0.eq)
            {
                R(state, 11) = 0xffffffff820d0000ull;
                R(state, 5) = R(state, 11) + 19612u;
                selected = true;
            }
        }
        if (!selected)
        {
            CompareUnsigned(state.cr6, R(state, 9), 0, state.xer_so);
            if (!state.cr6.eq)
            {
                R(state, 11) = 0xffffffffc0000000ull;
                CompareWords(state.cr6, R(state, 10), R(state, 11), state.xer_so);
                if (state.cr6.eq)
                {
                    CompareUnsigned(state.cr6, R(state, 5), 0, state.xer_so);
                    if (state.cr6.eq)
                    {
                        R(state, 11) = 0xffffffff820d0000ull;
                        R(state, 5) = R(state, 11) + 19604u;
                        selected = true;
                        five = true;
                    }
                }
            }
        }
        if (!selected)
        {
            CompareWords(state.cr6, R(state, 10), R(state, 14), state.xer_so);
            if (state.cr6.eq)
            {
                CompareUnsigned(state.cr6, R(state, 5), 0, state.xer_so);
                if (state.cr6.eq)
                {
                    R(state, 11) = 0xffffffff820d0000ull;
                    R(state, 5) = R(state, 11) + 19596u;
                    selected = true;
                    five = true;
                }
            }
        }
        if (!selected)
        {
            R(state, 11) = 0xffffffff820d0000ull;
            R(state, 5) = R(state, 11) + 19588u;
        }
        R(state, 4) = 22;
        R(state, 3) = R(state, 31) + 4u;
        state.lr = five ? 0x8231a5ccu : 0x8231a608u;
        (void)crt_float_core_helpers::Apply(0x8231b0d0u, memory,
            dependencies.helpers, state);
        CompareSigned(state.cr0, R(state, 3), 0, state.xer_so);
        if (!state.cr0.eq)
        {
            for (unsigned index = 3; index <= 7; ++index)
                R(state, index) = 0;
            CallFatal(memory, dependencies, state,
                five ? 0x8231a5ecu : 0x8231a628u);
        }
        R(state, 11) = five ? 5u : 6u;
        R(state, 3) = 0;
        memory.WriteU8(Address(R(state, 31)) + 3u, static_cast<std::uint8_t>(R(state, 11)));
        RestoreFrame(memory, state, 14, 320);
        return;
    }

    SelectPowers(memory, state);
    R(state, 9) = memory.ReadU16(sp + 88u);
    CompareUnsigned(state.cr6, R(state, 9), 16383u, state.xer_so);
    if (!state.cr6.lt)
    {
        R(state, 8) = R(state, 24) + 1u;
        R(state, 11) = memory.ReadU16(sp + 136u);
        R(state, 10) = W(R(state, 9)) & 0x7fffu;
        R(state, 19) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int16_t>(R(state, 8))));
        MultiplyAndNormalize(memory, state, sp + 136u, true);
    }
    R(state, 11) = memory.ReadU32(sp + 364u);
    memory.WriteU16(Address(R(state, 31)), H(R(state, 19)));
    R(state, 11) = W(R(state, 11)) & 1u;
    CompareSigned(state.cr0, R(state, 11), 0, state.xer_so);
    if (!state.cr0.eq)
    {
        R(state, 10) = memory.ReadU32(sp + 356u);
        R(state, 11) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(static_cast<std::int16_t>(R(state, 19))));
        R(state, 11) += R(state, 10);
        const auto signed_sum = static_cast<std::int64_t>(R(state, 11));
        state.cr0 = {std::uint8_t(signed_sum < 0),
            std::uint8_t(signed_sum > 0), std::uint8_t(signed_sum == 0),
            state.xer_so};
        memory.WriteU32(sp + 356u, W(R(state, 11)));
        if (!state.cr0.gt)
        {
            R(state, 11) = memory.ReadU32(sp + 80u);
            memory.WriteU16(Address(R(state, 31)), 0);
            CompareUnsigned(state.cr6, R(state, 11), 0x8000u, state.xer_so);
            R(state, 11) = state.cr6.eq ? 45u : 32u;
            memory.WriteU8(Address(R(state, 31)) + 2u,
                static_cast<std::uint8_t>(R(state, 11)));
            R(state, 11) = 1;
            memory.WriteU8(Address(R(state, 31)) + 3u, 1);
            memory.WriteU8(Address(R(state, 31)) + 5u, 0);
            R(state, 3) = 1;
            R(state, 10) = 48;
            memory.WriteU8(Address(R(state, 31)) + 4u, 48);
            RestoreFrame(memory, state, 14, 320);
            return;
        }
    }
    else
        R(state, 11) = memory.ReadU32(sp + 356u);
    CompareSigned(state.cr6, R(state, 11), 21, state.xer_so);
    if (state.cr6.gt)
    {
        R(state, 11) = 21;
        memory.WriteU32(sp + 356u, 21);
    }
    AlignMantissaForDigits(memory, state);
    ProduceDecimalDigits(memory, state);
    FinalizeDecimalRecord(memory, state);
    RestoreFrame(memory, state, 14, 320);
}

void ConvertBinary64(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    SaveFrame(memory, state, 29, 160);
    const auto sp = Address(state.sp);
    WriteU64(memory, sp + 176u, R(state, 3));
    R(state, 31) = R(state, 4);
    R(state, 4) = state.sp + 176u;
    R(state, 3) = state.sp + 80u;
    R(state, 30) = R(state, 5);
    R(state, 29) = R(state, 6);
    state.lr = 0x8231a318u;
    (void)crt_float_core_helpers::Apply(0x8231a390u, memory,
        dependencies.helpers, state);
    R(state, 11) = memory.ReadU16(sp + 88u);
    R(state, 7) = state.sp + 96u;
    R(state, 6) = 0;
    R(state, 3) = ReadU64(memory, sp + 80u);
    R(state, 5) = 17;
    R(state, 4) = static_cast<std::uint64_t>(R(state, 11) << 48u);
    state.lr = 0x8231a334u;
    ConvertExtended(memory, dependencies, state);
    R(state, 10) = memory.ReadU8(sp + 98u);
    R(state, 11) = R(state, 3);
    R(state, 10) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int8_t>(R(state, 10))));
    R(state, 5) = state.sp + 100u;
    R(state, 4) = R(state, 29);
    R(state, 3) = R(state, 30);
    memory.WriteU32(Address(R(state, 31)) + 8u, W(R(state, 11)));
    memory.WriteU32(Address(R(state, 31)), W(R(state, 10)));
    R(state, 9) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int16_t>(memory.ReadU16(sp + 96u))));
    memory.WriteU32(Address(R(state, 31)) + 4u, W(R(state, 9)));
    state.lr = 0x8231a360u;
    (void)crt_float_core_helpers::Apply(0x8231b0d0u, memory,
        dependencies.helpers, state);
    CompareSigned(state.cr0, R(state, 3), 0, state.xer_so);
    if (!state.cr0.eq)
    {
        for (unsigned index = 3; index <= 7; ++index)
            R(state, index) = 0;
        CallFatal(memory, dependencies, state, 0x8231a380u);
    }
    R(state, 3) = R(state, 31);
    memory.WriteU32(Address(R(state, 31)) + 12u, W(R(state, 30)));
    RestoreFrame(memory, state, 29, 160);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (entry)
    {
    case 0x8231a470u: ConvertExtended(memory, dependencies, registers); return true;
    case 0x8231a2f0u: ConvertBinary64(memory, dependencies, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::crt_float_conversion
