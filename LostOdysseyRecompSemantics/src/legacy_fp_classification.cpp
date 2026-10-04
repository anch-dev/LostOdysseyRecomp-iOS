#include "lo_semantics/legacy_fp_classification.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::legacy_fp_classification
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{ return index == 1u ? state.integer.sp : state.integer.r[index]; }

void CompareUnsigned(crt_stream_operations::Condition& cr,
    std::uint64_t left, std::uint32_t right, std::uint8_t so)
{
    const auto value = static_cast<std::uint32_t>(left);
    cr = {std::uint8_t(value < right), std::uint8_t(value > right),
        std::uint8_t(value == right), so};
}

void CompareSigned(crt_stream_operations::Condition& cr,
    std::uint64_t left, std::int32_t right, std::uint8_t so)
{
    const auto value = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left));
    cr = {std::uint8_t(value < right), std::uint8_t(value > right),
        std::uint8_t(value == right), so};
}

void CompareFp(Registers& state)
{
    const double left = std::bit_cast<double>(state.f1_bits);
    const double right = std::bit_cast<double>(state.f0_bits);
    const bool unordered = std::isnan(left) || std::isnan(right);
    state.integer.cr6 = {std::uint8_t(!unordered && left < right),
        std::uint8_t(!unordered && left > right),
        std::uint8_t(!unordered && left == right),
        std::uint8_t(unordered)};
}

void DisableFlush(Registers& state, HostFpServices& services)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if (state.cached_fp_control & FlushMask)
    {
        state.cached_fp_control &= ~FlushMask;
        services.SetHostFpControl(state.cached_fp_control);
    }
}

void StoreInput(GuestMemory& memory, HostFpServices& services,
    Registers& state)
{
    DisableFlush(state, services);
    WriteU64(memory, Address(state.integer.sp + 16u), state.f1_bits);
}

void FiniteExponent(GuestMemory& memory, HostFpServices& services,
    Registers& state)
{
    StoreInput(memory, services, state);
    R(state, 11) = memory.ReadU16(Address(R(state, 1) + 16u));
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x7ff0u);
    R(state, 11) -= 32752u;
    R(state, 11) = std::countl_zero(static_cast<std::uint32_t>(R(state, 11)));
    R(state, 11) = WordRotateMask(R(state, 11), 27, 1u);
    R(state, 3) = R(state, 11) ^ 1u;
}

void IsNan(GuestMemory& memory, HostFpServices& services,
    Registers& state)
{
    auto& integer = state.integer;
    StoreInput(memory, services, state);
    R(state, 11) = memory.ReadU16(Address(R(state, 1) + 16u));
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x7ff8u);
    CompareUnsigned(integer.cr6, R(state, 11), 32752u, integer.xer_so);
    if (integer.cr6.eq)
    {
        R(state, 10) = memory.ReadU32(Address(R(state, 1) + 16u));
        R(state, 10) &= 0x7ffffu;
        CompareSigned(integer.cr0, R(state, 10), 0, integer.xer_so);
        if (!integer.cr0.eq) { R(state, 3) = 1u; return; }
        R(state, 10) = memory.ReadU32(Address(R(state, 1) + 20u));
        CompareUnsigned(integer.cr6, R(state, 10), 0u, integer.xer_so);
        if (!integer.cr6.eq) { R(state, 3) = 1u; return; }
    }
    CompareUnsigned(integer.cr6, R(state, 11), 32760u, integer.xer_so);
    R(state, 3) = integer.cr6.eq ? 1u : 0u;
}

void SpecialKind(GuestMemory& memory, HostFpServices& services,
    Registers& state)
{
    auto& integer = state.integer;
    StoreInput(memory, services, state);
    R(state, 11) = 2146435072u;
    R(state, 10) = memory.ReadU32(Address(R(state, 1) + 16u));
    R(state, 9) = memory.ReadU32(Address(R(state, 1) + 20u));
    CompareUnsigned(integer.cr6, R(state, 10),
        static_cast<std::uint32_t>(R(state, 11)), integer.xer_so);
    if (integer.cr6.eq)
    {
        CompareUnsigned(integer.cr6, R(state, 9), 0u, integer.xer_so);
        if (integer.cr6.eq) { R(state, 3) = 1u; return; }
    }
    else
    {
        R(state, 11) = static_cast<std::uint64_t>(
            std::int64_t{-1048576});
        CompareUnsigned(integer.cr6, R(state, 10),
            static_cast<std::uint32_t>(R(state, 11)), integer.xer_so);
        if (integer.cr6.eq)
        {
            CompareUnsigned(integer.cr6, R(state, 9), 0u, integer.xer_so);
            if (integer.cr6.eq) { R(state, 3) = 2u; return; }
        }
    }
    R(state, 11) = memory.ReadU16(Address(R(state, 1) + 16u));
    R(state, 11) = WordRotateMask(R(state, 11), 0, 0x7ff8u);
    CompareUnsigned(integer.cr6, R(state, 11), 32760u, integer.xer_so);
    if (integer.cr6.eq) { R(state, 3) = 3u; return; }
    CompareUnsigned(integer.cr6, R(state, 11), 32752u, integer.xer_so);
    if (integer.cr6.eq)
    {
        R(state, 11) = R(state, 10) & 0x7ffffu;
        CompareSigned(integer.cr0, R(state, 11), 0, integer.xer_so);
        if (!integer.cr0.eq) { R(state, 3) = 4u; return; }
        CompareUnsigned(integer.cr6, R(state, 9), 0u, integer.xer_so);
        if (!integer.cr6.eq) { R(state, 3) = 4u; return; }
    }
    R(state, 3) = 0u;
}

void SignMask(Registers& state)
{
    const bool negative = (R(state, 11) & 0x8000u) != 0u;
    R(state, 11) = negative ? ~std::uint64_t{0} : 0u;
    state.integer.xer_ca = std::uint8_t(!negative);
}

void Category(GuestMemory& memory, HostFpServices& services,
    Registers& state)
{
    auto& integer = state.integer;
    const auto incoming_sp = integer.sp;
    R(state, 12) = integer.lr;
    memory.WriteU32(Address(incoming_sp - 8u), Address(R(state, 12)));
    memory.WriteU32(Address(incoming_sp - 96u), Address(incoming_sp));
    integer.sp -= 96u;
    DisableFlush(state, services);
    WriteU64(memory, Address(integer.sp + 112u), state.f1_bits);
    R(state, 11) = memory.ReadU16(Address(integer.sp + 112u));
    R(state, 10) = WordRotateMask(R(state, 11), 0, 0x7ff0u);
    CompareUnsigned(integer.cr6, R(state, 10), 32752u, integer.xer_so);
    if (integer.cr6.eq)
    {
        integer.lr = 0x82b7dfe4u;
        SpecialKind(memory, services, state);
        CompareSigned(integer.cr6, R(state, 3), 1, integer.xer_so);
        if (integer.cr6.eq) R(state, 3) = 512u;
        else
        {
            CompareSigned(integer.cr6, R(state, 3), 2, integer.xer_so);
            if (integer.cr6.eq) R(state, 3) = 4u;
            else
            {
                R(state, 11) = R(state, 3) - 3u;
                R(state, 11) = std::countl_zero(
                    static_cast<std::uint32_t>(R(state, 11)));
                R(state, 11) = WordRotateMask(R(state, 11), 27, 1u);
                R(state, 3) = R(state, 11) + 1u;
            }
        }
    }
    else
    {
        R(state, 11) = WordRotateMask(R(state, 11), 0, 0x8000u);
        CompareUnsigned(integer.cr6, R(state, 10), 0u, integer.xer_so);
        bool nonzero_mantissa = false;
        if (integer.cr6.eq)
        {
            R(state, 10) = memory.ReadU32(Address(integer.sp + 112u));
            R(state, 10) &= 0xfffffu;
            CompareSigned(integer.cr0, R(state, 10), 0, integer.xer_so);
            if (!integer.cr0.eq) nonzero_mantissa = true;
            else
            {
                R(state, 10) = memory.ReadU32(Address(integer.sp + 116u));
                CompareUnsigned(integer.cr6, R(state, 10), 0u,
                    integer.xer_so);
                nonzero_mantissa = !integer.cr6.eq;
            }
        }
        SignMask(state);
        if (nonzero_mantissa)
        {
            R(state, 11) = WordRotateMask(R(state, 11), 0, 0xfffffff0u);
            R(state, 11) = WordRotateMask(R(state, 11), 0,
                0xffffffffffffff9full);
            R(state, 3) = R(state, 11) + 128u;
        }
        else
        {
            R(state, 10) = static_cast<std::uint64_t>(
                std::int64_t{-2113929216});
            DisableFlush(state, services);
            state.f0_bits = ReadU64(memory, Address(R(state, 10) + 4072u));
            CompareFp(state);
            if (integer.cr6.eq)
            {
                R(state, 11) = WordRotateMask(R(state, 11), 0,
                    0xffffffe0u);
                R(state, 3) = R(state, 11) + 64u;
            }
            else
            {
                R(state, 11) = WordRotateMask(R(state, 11), 0,
                    0xfffffff8u);
                R(state, 11) = WordRotateMask(R(state, 11), 0,
                    0xffffffffffffff0full);
                R(state, 3) = R(state, 11) + 256u;
            }
        }
    }
    integer.sp += 96u;
    R(state, 12) = memory.ReadU32(Address(integer.sp - 8u));
    integer.lr = R(state, 12);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    HostFpServices& services, Registers& registers)
{
    switch (entry)
    {
    case 0x82b7df58u: FiniteExponent(memory, services, registers); return true;
    case 0x82b7df78u: IsNan(memory, services, registers); return true;
    case 0x82b7dfc0u: Category(memory, services, registers); return true;
    case 0x82b82340u: SpecialKind(memory, services, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_fp_classification
