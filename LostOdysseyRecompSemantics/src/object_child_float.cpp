#include "lo_semantics/object_child_float.h"
#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>

namespace lo::semantic::gpu::object_child_float
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr std::uint64_t ConstantBase = 0xffffffff82000000ull;

void DisableFlush(NativeServices& native, Registers& state)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if ((state.cached_fp_control & FlushMask) != 0u)
    {
        state.cached_fp_control &= ~FlushMask;
        native.SetHostFpControl(state.cached_fp_control);
    }
}
std::uint64_t LoadSingle(GuestMemory& memory, NativeServices& native,
    Registers& state, GuestAddress address)
{
    DisableFlush(native, state);
    return LoadedSingle::FromWord(memory.ReadU32(address)).FprBits();
}
void CompareSigned(Registers& state, std::uint64_t left, std::int32_t right)
{
    const auto value = static_cast<std::int32_t>(left);
    state.cr6 = {std::uint8_t(value < right), std::uint8_t(value > right),
        std::uint8_t(value == right), state.xer_so};
}
void CompareUnsignedZero(Registers& state, std::uint64_t value)
{
    const auto word = static_cast<std::uint32_t>(value);
    state.cr6 = {0, std::uint8_t(word != 0u),
        std::uint8_t(word == 0u), state.xer_so};
}
void CompareFloat(Registers& state, std::uint64_t left_bits,
    std::uint64_t right_bits)
{
    const auto left = std::bit_cast<double>(left_bits);
    const auto right = std::bit_cast<double>(right_bits);
    const bool un = std::isnan(left) || std::isnan(right);
    state.cr6 = {std::uint8_t(!un && left < right),
        std::uint8_t(!un && left > right),
        std::uint8_t(!un && left == right), std::uint8_t(un)};
}
void SaveGprs(GuestMemory& memory, Registers& state, unsigned first)
{
    const auto sp = Address(state.r[1]);
    for (unsigned index = first; index <= 31u; ++index)
        WriteU64(memory, sp - 16u - (31u - index) * 8u, state.r[index]);
    memory.WriteU32(sp - 8u, Address(state.r[12]));
}
void RestoreGprs(GuestMemory& memory, Registers& state, unsigned first)
{
    const auto sp = Address(state.r[1]);
    for (unsigned index = first; index <= 31u; ++index)
        state.r[index] = ReadU64(memory, sp - 16u - (31u - index) * 8u);
    state.r[12] = memory.ReadU32(sp - 8u);
    state.lr = state.r[12];
}
void PushFrame(GuestMemory& memory, Registers& state, std::uint32_t size)
{
    const auto previous_sp = Address(state.r[1]);
    state.r[1] -= size;
    memory.WriteU32(Address(state.r[1]), previous_sp);
}
void ReturnChild(GuestMemory& memory, Registers& state)
{
    state.r[1] += 128u;
    state.f31_bits = ReadU64(memory, Address(state.r[1] - 40u));
    RestoreGprs(memory, state, 29u);
}
void ReturnParent(GuestMemory& memory, Registers& state)
{
    state.r[1] += 144u;
    state.f30_bits = ReadU64(memory, Address(state.r[1] - 64u));
    state.f31_bits = ReadU64(memory, Address(state.r[1] - 56u));
    RestoreGprs(memory, state, 27u);
}

void EvaluateChild(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    state.r[12] = state.lr;
    state.lr = 0x822c5f30u;
    SaveGprs(memory, state, 29u);
    DisableFlush(native, state);
    WriteU64(memory, Address(state.r[1] - 40u), state.f31_bits);
    PushFrame(memory, state, 128u);

    state.r[11] = memory.ReadU32(Address(state.r[3] + 220u));
    CompareSigned(state, state.r[11], 0);
    if (state.cr6.gt != 0u)
    {
        state.r[11] = memory.ReadU32(Address(state.r[3] + 216u));
        state.r[31] = memory.ReadU32(Address(state.r[11]));
    }
    else
        state.r[31] = 0u;
    state.r[11] = memory.ReadU32(Address(state.r[31] + 72u));

    CompareSigned(state, state.r[4], 0);
    if (state.cr6.eq != 0u)
    {
        state.r[11] = memory.ReadU32(Address(state.r[11] + 80u));
        CompareSigned(state, state.r[11], 0);
        if (state.cr6.eq != 0u)
        {
            state.r[11] = ConstantBase;
            state.f1_bits = LoadSingle(memory, native, state,
                Address(state.r[11] + 3648u));
            ReturnChild(memory, state);
            return;
        }
        state.r[11] = memory.ReadU32(Address(state.r[31] + 72u));
    }
    else
    {
        state.r[10] = memory.ReadU32(Address(state.r[11] + 80u));
        CompareSigned(state, state.r[10], 0);
        if (state.cr6.eq != 0u)
        {
            state.f31_bits = LoadSingle(memory, native, state,
                Address(state.r[11] + 72u));
            goto after_scale;
        }
        state.r[11] = static_cast<std::uint32_t>(state.r[11]);
    }

    state.r[10] = memory.ReadU32(Address(state.r[11] + 80u));
    state.f0_bits = LoadSingle(memory, native, state,
        Address(state.r[11] + 72u));
    state.r[11] = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(static_cast<std::int32_t>(state.r[10])));
    WriteU64(memory, Address(state.r[1] + 80u), state.r[11]);
    state.f13_bits = ReadU64(memory, Address(state.r[1] + 80u));
    DisableFlush(native, state);
    state.f13_bits = std::bit_cast<std::uint64_t>(double(
        std::bit_cast<std::int64_t>(state.f13_bits)));
    state.f13_bits = std::bit_cast<std::uint64_t>(double(float(
        std::bit_cast<double>(state.f13_bits))));
    state.f31_bits = std::bit_cast<std::uint64_t>(double(float(
        std::bit_cast<double>(state.f13_bits) *
        std::bit_cast<double>(state.f0_bits))));

after_scale:
    CompareSigned(state, state.r[5], 0);
    if (state.cr6.eq != 0u)
    {
        state.r[11] = memory.ReadU32(Address(state.r[31] + 80u));
        state.r[29] = 0u;
        CompareSigned(state, state.r[11], 0);
        if (state.cr6.gt != 0u)
        {
            state.r[30] = 0u;
            while (true)
            {
                state.r[11] = memory.ReadU32(Address(state.r[31] + 76u));
                state.r[3] = memory.ReadU32(Address(state.r[11] + state.r[30]));
                state.lr = 0x822c5ffcu;
                native.CallGuest(0x82384c08u, memory, state);
                CompareUnsignedZero(state, state.r[3]);
                if (state.cr6.eq == 0u)
                {
                    state.r[11] = memory.ReadU32(Address(state.r[3]));
                    state.r[11] = memory.ReadU32(Address(state.r[11] + 332u));
                    state.ctr = state.r[11];
                    state.lr = 0x822c6038u;
                    native.CallGuest(Address(state.ctr) & ~3u, memory, state);
                    DisableFlush(native, state);
                    state.f31_bits = std::bit_cast<std::uint64_t>(double(float(
                        std::bit_cast<double>(state.f1_bits) +
                        std::bit_cast<double>(state.f31_bits))));
                    break;
                }
                state.r[11] = memory.ReadU32(Address(state.r[31] + 80u));
                state.r[29] += 1u;
                state.r[30] += 4u;
                CompareSigned(state, state.r[29],
                    static_cast<std::int32_t>(state.r[11]));
                if (state.cr6.lt == 0u)
                    break;
            }
        }
    }
    DisableFlush(native, state);
    state.f1_bits = state.f31_bits;
    ReturnChild(memory, state);
}

void EvaluateParent(GuestMemory& memory, NativeServices& native,
    Registers& state)
{
    state.r[12] = state.lr;
    state.lr = 0x822c5e60u;
    SaveGprs(memory, state, 27u);
    DisableFlush(native, state);
    WriteU64(memory, Address(state.r[1] - 64u), state.f30_bits);
    WriteU64(memory, Address(state.r[1] - 56u), state.f31_bits);
    PushFrame(memory, state, 144u);
    state.r[11] = ConstantBase;
    state.r[28] = state.r[4];
    state.r[30] = state.r[3];
    state.r[27] = state.r[5];
    CompareSigned(state, state.r[28], 0);
    state.f30_bits = LoadSingle(memory, native, state,
        Address(state.r[11] + 3664u));
    state.f31_bits = state.f30_bits;

    if (state.cr6.eq != 0u)
    {
        state.r[11] = memory.ReadU32(Address(state.r[30] + 244u));
        state.r[11] = memory.ReadU32(Address(state.r[11] + 60u));
        state.r[11] &= 0x80000000u;
        CompareUnsignedZero(state, state.r[11]);
        if (state.cr6.eq == 0u)
        {
            state.r[11] = ConstantBase;
            state.f1_bits = LoadSingle(memory, native, state,
                Address(state.r[11] + 3648u));
            ReturnParent(memory, state);
            return;
        }
    }

    state.r[11] = memory.ReadU32(Address(state.r[30] + 120u));
    state.r[29] = 0u;
    CompareSigned(state, state.r[11], 0);
    if (state.cr6.gt != 0u)
    {
        state.r[31] = 0u;
        while (true)
        {
            state.r[11] = memory.ReadU32(Address(state.r[30] + 116u));
            state.r[3] = memory.ReadU32(Address(state.r[11] + state.r[31]));
            CompareUnsignedZero(state, state.r[3]);
            if (state.cr6.eq == 0u)
            {
                state.r[5] = state.r[27];
                state.r[4] = state.r[28];
                state.lr = 0x822c5ee8u;
                EvaluateChild(memory, native, state);
                CompareFloat(state, state.f1_bits, state.f30_bits);
                if (state.cr6.lt != 0u)
                {
                    state.r[11] = ConstantBase;
                    state.f1_bits = LoadSingle(memory, native, state,
                        Address(state.r[11] + 3648u));
                    ReturnParent(memory, state);
                    return;
                }
                CompareFloat(state, state.f1_bits, state.f31_bits);
                if (state.cr6.gt != 0u)
                    state.f31_bits = state.f1_bits;
            }
            state.r[11] = memory.ReadU32(Address(state.r[30] + 120u));
            state.r[29] += 1u;
            state.r[31] += 4u;
            CompareSigned(state, state.r[29],
                static_cast<std::int32_t>(state.r[11]));
            if (state.cr6.lt == 0u)
                break;
        }
    }
    DisableFlush(native, state);
    state.f1_bits = state.f31_bits;
    ReturnParent(memory, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    switch (entry)
    {
    case 0x822c5e58u: EvaluateParent(memory, native, state); return true;
    case 0x822c5f28u: EvaluateChild(memory, native, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::object_child_float
