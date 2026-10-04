#include "lo_semantics/legacy_descriptor_array_allocation.h"

#include "lo_semantics/legacy_descriptor_mutation_routes.h"
#include "lo_semantics/legacy_descriptor_record_routes.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_array_allocation
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Integer = crt_stream_operations::Registers;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Integer& state, unsigned index)
{ return index == 1u ? state.sp : state.r[index]; }

void CompareU(Integer& state, std::uint64_t left, std::uint64_t right,
    Condition& cr)
{
    const auto l = static_cast<std::uint32_t>(left);
    const auto r = static_cast<std::uint32_t>(right);
    cr = {std::uint8_t(l < r), std::uint8_t(l > r),
        std::uint8_t(l == r), state.xer_so};
}

void CompareS(Integer& state, std::uint64_t left, std::uint64_t right,
    Condition& cr)
{
    const auto l = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    const auto r = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(right));
    cr = {std::uint8_t(l < r), std::uint8_t(l > r),
        std::uint8_t(l == r), state.xer_so};
}

void CompareFp(Registers& state, double left, double right)
{
    auto& cr = state.integer.cr6;
    const bool unordered = std::isnan(left) || std::isnan(right);
    cr = {std::uint8_t(!unordered && left < right),
        std::uint8_t(!unordered && left > right),
        std::uint8_t(!unordered && left == right),
        std::uint8_t(unordered)};
}

void ReadWord(GuestMemory& memory, Integer& state, unsigned target,
    std::uint64_t address)
{ R(state, target) = memory.ReadU32(Address(address)); }

void WriteWord(GuestMemory& memory, std::uint64_t address,
    std::uint64_t value)
{ memory.WriteU32(Address(address), static_cast<std::uint32_t>(value)); }

void Rlwimi(std::uint64_t& destination, std::uint64_t source,
    int rotation, std::uint64_t mask)
{ destination = WordRotateMask(source, rotation, mask) |
    (destination & ~mask); }

void DisableFlush(Registers& state, Services& services)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if (state.cached_fp_control & FlushMask)
    {
        state.cached_fp_control &= ~FlushMask;
        services.SetHostFpControl(state.cached_fp_control);
    }
}

double LoadFloat(GuestMemory& memory, Registers& state,
    Services& services, std::uint64_t address)
{
    DisableFlush(state, services);
    const auto bits = memory.ReadU32(Address(address));
    return static_cast<double>(std::bit_cast<float>(bits));
}

void FillAccepted(GuestMemory& memory, Integer& state)
{
    const auto destination = Address(R(state, 3));
    const auto bytes = static_cast<std::uint32_t>(R(state, 5));
    const auto padding = (0u - destination) & 3u;
    const auto prefix = bytes < padding ? bytes : padding;
    const auto remaining = bytes - prefix;
    (void)FillGuestMemory(memory, destination,
        static_cast<std::uint8_t>(R(state, 4)), bytes);
    R(state, 6) = R(state, 3) + prefix + (remaining & ~3u);
    R(state, 5) -= prefix;
    R(state, 4) = (R(state, 4) & 0xffffffff00000000ull) |
        (std::uint32_t{static_cast<std::uint8_t>(R(state, 4))} *
            0x01010101u);
    const auto tail = remaining & 3u;
    R(state, 0) = tail;
    CompareS(state, tail, 0u, state.cr0);
    state.ctr = tail == 3u ? 1u : 0u;
}

void AllocateRecord(GuestMemory& memory, Services& services,
    Integer& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    WriteWord(memory, incoming_sp - 8u, R(state, 12));
    WriteU64(memory, Address(incoming_sp - 16u), R(state, 31));
    WriteWord(memory, incoming_sp - 96u, incoming_sp);
    state.sp -= 96u;
    R(state, 11) = R(state, 3) + 772u;
    ReadWord(memory, state, 10, R(state, 11) + 144u);
    ReadWord(memory, state, 9, R(state, 11) + 140u);
    R(state, 9) -= R(state, 10);
    R(state, 9) += 4096u;
    CompareU(state, R(state, 9), R(state, 4), state.cr6);
    if (!state.cr6.lt)
    {
        R(state, 9) = R(state, 10) + R(state, 4);
        R(state, 3) = R(state, 10);
        WriteWord(memory, R(state, 11) + 144u, R(state, 9));
    }
    else
    {
        R(state, 10) = WordRotateMask(R(state, 4), 30, 0x3fffffffu);
        R(state, 10) -= 1u;
        R(state, 10) = WordRotateMask(R(state, 10), 2, 0xfffffffcu);
        ReadWord(memory, state, 31, R(state, 10) + R(state, 11));
        CompareU(state, R(state, 31), 0u, state.cr0);
        if (!state.cr0.eq)
        {
            ReadWord(memory, state, 9, R(state, 31));
            R(state, 5) = R(state, 4);
            R(state, 4) = 0u;
            R(state, 3) = R(state, 31);
            WriteWord(memory, R(state, 10) + R(state, 11), R(state, 9));
            state.lr = 0x82fb371cu;
            FillAccepted(memory, state);
            R(state, 3) = R(state, 31);
        }
        else
        {
            R(state, 3) = R(state, 11);
            state.lr = 0x82fb372cu;
            services.AllocateFromPool(memory, state);
        }
    }
    state.sp += 96u;
    ReadWord(memory, state, 12, state.sp - 8u);
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void SaveConstructor(GuestMemory& memory, Integer& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    state.lr = 0x83058598u;
    for (unsigned index = 27u; index <= 31u; ++index)
        WriteU64(memory, Address(incoming_sp - 48u +
            (index - 27u) * 8u), R(state, index));
    WriteWord(memory, incoming_sp - 8u, R(state, 12));
    WriteWord(memory, incoming_sp - 144u, incoming_sp);
    state.sp -= 144u;
}

void RestoreConstructor(GuestMemory& memory, Integer& state)
{
    state.sp += 144u;
    for (unsigned index = 27u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory, Address(state.sp - 48u +
            (index - 27u) * 8u));
    ReadWord(memory, state, 12, state.sp - 8u);
    state.lr = R(state, 12);
}

void ConstructArray(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& g = state.integer;
    SaveConstructor(memory, g);
    R(g, 29) = R(g, 6);
    R(g, 30) = R(g, 4);
    R(g, 28) = R(g, 5);
    R(g, 6) = R(g, 30);
    R(g, 5) = 0u;
    R(g, 4) = 124u;
    WriteWord(memory, g.sp + 188u, R(g, 29));
    R(g, 31) = R(g, 3);
    g.lr = 0x830585c0u;
    (void)legacy_descriptor_record_routes::Apply(0x82fac238u, memory, g);
    R(g, 4) = R(g, 3);
    R(g, 3) = R(g, 31);
    R(g, 5) = 34u;
    g.lr = 0x830585d0u;
    AllocateRecord(memory, services, g);
    ReadWord(memory, g, 11, R(g, 31) + 4u);
    R(g, 27) = R(g, 3);
    R(g, 10) = R(g, 11) & 1u;
    CompareS(g, R(g, 10), 0u, g.cr0);
    if (!g.cr0.eq) R(g, 11) = 0u;
    R(g, 8) = R(g, 30);
    R(g, 7) = 0u;
    R(g, 6) = 124u;
    R(g, 5) = R(g, 11);
    R(g, 4) = R(g, 31);
    R(g, 3) = R(g, 27);
    g.lr = 0x83058600u;
    (void)legacy_descriptor_mutation_routes::Apply(0x83056568u, memory, g);
    CompareU(g, R(g, 30), 0u, g.cr6);
    if (!g.cr6.eq)
    {
        R(g, 10) = R(g, 28);
        R(g, 9) = R(g, 27) + 40u;
        R(g, 11) = R(g, 30);
        do
        {
            const auto input = LoadFloat(memory, state, services, R(g, 10));
            state.f0_bits = std::bit_cast<std::uint64_t>(input);
            g.xer_ca = std::uint8_t(static_cast<std::uint32_t>(R(g, 11)) > 0u);
            R(g, 11) -= 1u;
            CompareS(g, R(g, 11), 0u, g.cr0);
            WriteWord(memory, R(g, 9),
                std::bit_cast<std::uint32_t>(static_cast<float>(input)));
            R(g, 10) += 4u;
            R(g, 9) += 4u;
        } while (!g.cr0.eq);
    }
    R(g, 11) = static_cast<std::uint32_t>(R(g, 29)) & 0xffu;
    CompareS(g, R(g, 11), 0u, g.cr0);
    if (!g.cr0.eq)
    {
        ReadWord(memory, g, 10, R(g, 27) + 16u);
        Rlwimi(R(g, 10), R(g, 11), 14, 0x3fc000u);
        WriteWord(memory, R(g, 27) + 16u, R(g, 10));
    }
    ReadWord(memory, g, 11, R(g, 31) + 4u);
    R(g, 10) = R(g, 11) & 1u;
    CompareS(g, R(g, 10), 0u, g.cr0);
    if (!g.cr0.eq) R(g, 11) = 0u;
    R(g, 11) += 16u;
    R(g, 10) = WordRotateMask(R(g, 27), 0, 0xfffffffeu);
    R(g, 7) = R(g, 11) - 32u;
    R(g, 10) += 32u;
    R(g, 7) |= 1u;
    ReadWord(memory, g, 8, R(g, 11));
    R(g, 6) = R(g, 10) - 32u;
    R(g, 9) = R(g, 10) + 4u;
    WriteWord(memory, R(g, 10), R(g, 8));
    ReadWord(memory, g, 8, R(g, 11));
    R(g, 8) = WordRotateMask(R(g, 8), 0, 0xfffffffeu);
    WriteWord(memory, R(g, 8), R(g, 6));
    WriteWord(memory, R(g, 10) + 4u, R(g, 7));
    WriteWord(memory, R(g, 11), R(g, 9));
    ReadWord(memory, g, 11, R(g, 31) + 40u);
    R(g, 11) = WordRotateMask(R(g, 11), 0, 0x4000u);
    CompareS(g, R(g, 11), 0u, g.cr0);
    if (g.cr0.eq)
    {
        CompareU(g, R(g, 30), 0u, g.cr6);
        if (!g.cr6.eq)
        {
            R(g, 11) = static_cast<std::uint64_t>(
                std::int64_t{-2113929216});
            R(g, 6) = 0u;
            R(g, 8) = 0u;
            R(g, 7) = R(g, 28);
            state.f13_bits = std::bit_cast<std::uint64_t>(
                LoadFloat(memory, state, services, R(g, 11) + 3664u));
            do
            {
                R(g, 3) = g.sp + 188u;
                R(g, 4) = WordRotateMask(R(g, 8), 29, 0x1ffffffcu);
                R(g, 11) = R(g, 8) + 1u;
                R(g, 10) = static_cast<std::uint32_t>(R(g, 8)) & 31u;
                R(g, 9) = static_cast<std::uint32_t>(R(g, 11)) & 31u;
                R(g, 5) = UINT64_MAX;
                ReadWord(memory, g, 4, R(g, 4) + R(g, 3));
                R(g, 3) = 2u;
                R(g, 11) = 0u;
                R(g, 5) = (static_cast<std::uint8_t>(R(g, 10)) & 0x20u) ?
                    0u : static_cast<std::uint32_t>(R(g, 5)) <<
                        (static_cast<std::uint8_t>(R(g, 10)) & 0x3fu);
                R(g, 9) = (static_cast<std::uint8_t>(R(g, 9)) & 0x20u) ?
                    0u : static_cast<std::uint32_t>(R(g, 3)) <<
                        (static_cast<std::uint8_t>(R(g, 9)) & 0x3fu);
                R(g, 9) -= 1u;
                R(g, 9) &= R(g, 4);
                R(g, 9) &= R(g, 5);
                R(g, 10) = (static_cast<std::uint8_t>(R(g, 10)) & 0x20u) ?
                    0u : static_cast<std::uint32_t>(R(g, 9)) >>
                        (static_cast<std::uint8_t>(R(g, 10)) & 0x3fu);
                CompareS(g, R(g, 10), 0u, g.cr0);
                if (!g.cr0.eq)
                {
                    CompareU(g, R(g, 10), 1u, g.cr6);
                    ReadWord(memory, g, 10, R(g, 7));
                    R(g, 11) = 4u;
                    if (g.cr6.eq)
                    {
                        CompareS(g, R(g, 10), 0u, g.cr0);
                        if (!g.cr0.lt) R(g, 11) = 6u;
                        CompareS(g, R(g, 10), 0u, g.cr6);
                        if (!g.cr6.gt) R(g, 11) |= 1u;
                    }
                    else
                    {
                        R(g, 11) = 6u;
                        CompareU(g, R(g, 10), 0u, g.cr6);
                        if (g.cr6.eq) R(g, 11) = 7u;
                    }
                }
                else
                {
                    const auto source = LoadFloat(memory, state, services,
                        R(g, 7));
                    state.f0_bits = std::bit_cast<std::uint64_t>(source);
                    CompareFp(state, source,
                        std::bit_cast<double>(state.f13_bits));
                    if (!g.cr6.lt) R(g, 11) = 2u;
                    DisableFlush(state, services);
                    CompareFp(state, source,
                        std::bit_cast<double>(state.f13_bits));
                    if (!g.cr6.gt) R(g, 11) |= 1u;
                    R(g, 10) = g.sp + 80u;
                    state.f0_bits = std::bit_cast<std::uint64_t>(
                        LoadFloat(memory, state, services, R(g, 7)));
                    const auto value = std::bit_cast<double>(state.f0_bits);
                    std::int32_t converted = INT_MIN;
                    if (value > static_cast<double>(INT_MAX))
                        converted = INT_MAX;
                    else if (std::isfinite(value) &&
                        value >= static_cast<double>(INT_MIN))
                        converted = static_cast<std::int32_t>(value);
                    state.f12_bits = static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(converted));
                    WriteWord(memory, R(g, 10), state.f12_bits);
                    R(g, 10) = static_cast<std::uint64_t>(
                        static_cast<std::int64_t>(std::bit_cast<std::int32_t>(
                            memory.ReadU32(Address(g.sp + 80u)))));
                    WriteU64(memory, Address(g.sp + 88u), R(g, 10));
                    state.f12_bits = ReadU64(memory, Address(g.sp + 88u));
                    state.f12_bits = std::bit_cast<std::uint64_t>(
                        static_cast<double>(
                            static_cast<std::int64_t>(state.f12_bits)));
                    state.f12_bits = std::bit_cast<std::uint64_t>(
                        static_cast<double>(static_cast<float>(
                            std::bit_cast<double>(state.f12_bits))));
                    CompareFp(state, std::bit_cast<double>(state.f12_bits),
                        std::bit_cast<double>(state.f0_bits));
                    if (g.cr6.eq) R(g, 11) |= 4u;
                }
                ReadWord(memory, g, 10, R(g, 27) + 16u);
                R(g, 11) = (static_cast<std::uint8_t>(R(g, 6)) & 0x20u) ?
                    0u : static_cast<std::uint32_t>(R(g, 11)) <<
                        (static_cast<std::uint8_t>(R(g, 6)) & 0x3fu);
                R(g, 9) = R(g, 10) & R(g, 11);
                R(g, 9) = static_cast<std::uint32_t>(R(g, 9)) & 0xfffu;
                CompareU(g, R(g, 9), R(g, 11), g.cr6);
                if (!g.cr6.eq)
                {
                    R(g, 11) = static_cast<std::uint32_t>(R(g, 11)) &
                        0xfffu;
                    R(g, 11) |= R(g, 10);
                    WriteWord(memory, R(g, 27) + 16u, R(g, 11));
                }
                g.xer_ca = std::uint8_t(
                    static_cast<std::uint32_t>(R(g, 30)) > 0u);
                R(g, 30) -= 1u;
                CompareS(g, R(g, 30), 0u, g.cr0);
                R(g, 8) += 2u;
                R(g, 7) += 4u;
                R(g, 6) += 3u;
            } while (!g.cr0.eq);
        }
    }
    R(g, 3) = R(g, 27);
    RestoreConstructor(memory, g);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Services& services,
    Registers& registers)
{
    switch (entry)
    {
    case 0x82fb36b0u:
        AllocateRecord(memory, services, registers.integer); return true;
    case 0x83058590u:
        ConstructArray(memory, services, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_array_allocation
