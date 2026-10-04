#include "lo_semantics/legacy_fp_numeric_routes.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::legacy_fp_numeric_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Condition = crt_stream_operations::Condition;
using Services = legacy_fp_classification::HostFpServices;

std::uint64_t& R(Registers& state, unsigned index)
{ return index == 1u ? state.classifier.integer.sp :
    state.classifier.integer.r[index]; }

void DisableFlush(Registers& state, Services& services)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    auto& control = state.classifier.cached_fp_control;
    if (control & FlushMask)
    {
        control &= ~FlushMask;
        services.SetHostFpControl(control);
    }
}

void CompareSigned(Condition& cr, std::uint64_t value,
    std::int32_t right, std::uint8_t so)
{
    const auto left = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(value));
    cr = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), so};
}

void CompareSigned64(Condition& cr, std::uint64_t value,
    std::int64_t right, std::uint8_t so)
{
    const auto left = std::bit_cast<std::int64_t>(value);
    cr = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), so};
}

void CompareFp(Condition& cr, std::uint64_t left_bits,
    std::uint64_t right_bits)
{
    constexpr std::uint64_t Exponent = 0x7ff0000000000000ull;
    constexpr std::uint64_t Fraction = 0x000fffffffffffffull;
    if (((left_bits & Exponent) == Exponent &&
            (left_bits & Fraction) != 0u) ||
        ((right_bits & Exponent) == Exponent &&
            (right_bits & Fraction) != 0u))
    {
        cr = {0u, 0u, 0u, 1u};
        return;
    }
    const double left = std::bit_cast<double>(left_bits);
    const double right = std::bit_cast<double>(right_bits);
    cr = {std::uint8_t(left < right), std::uint8_t(left > right),
        std::uint8_t(left == right), 0u};
}

void LoadSingle(GuestMemory& memory, Services& services,
    Registers& state, std::uint64_t address, std::uint64_t& result)
{
    DisableFlush(state, services);
    const auto bits = memory.ReadU32(Address(address));
    result = std::bit_cast<std::uint64_t>(
        static_cast<double>(std::bit_cast<float>(bits)));
}

void EnterFrame(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.classifier.integer;
    R(state, 12) = integer.lr;
    memory.WriteU32(Address(integer.sp - 8u),
        static_cast<std::uint32_t>(R(state, 12)));
    DisableFlush(state, services);
    WriteU64(memory, Address(integer.sp - 16u), state.f31_bits);
    memory.WriteU32(Address(integer.sp - 112u),
        static_cast<std::uint32_t>(integer.sp));
    integer.sp -= 112u;
    state.f31_bits = state.classifier.f1_bits;
}

void LeaveFrame(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.classifier.integer;
    integer.sp += 112u;
    R(state, 12) = memory.ReadU32(Address(integer.sp - 8u));
    integer.lr = R(state, 12);
    DisableFlush(state, services);
    state.f31_bits = ReadU64(memory, Address(integer.sp - 16u));
}

void Classify(GuestMemory& memory, Services& services,
    Registers& state, GuestAddress return_address)
{
    state.classifier.integer.lr = return_address;
    (void)legacy_fp_classification::Apply(0x82b7dfc0u, memory,
        services, state.classifier);
}

void NormalizeNonzero(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.classifier.integer;
    auto& cr6 = integer.cr6;
    auto& f1 = state.classifier.f1_bits;
    DisableFlush(state, services);
    WriteU64(memory, Address(integer.sp + 16u), f1);
    WriteU64(memory, Address(integer.sp - 16u), f1);
    R(state, 11) = ReadU64(memory, Address(integer.sp - 16u));
    R(state, 10) = R(state, 11) & 0x7fffffffffffffffull;
    CompareSigned64(cr6, R(state, 10), 0, integer.xer_so);
    if (!cr6.eq)
    {
        R(state, 12) = 2047u;
        R(state, 12) = (R(state, 12) << 52) &
            0xfff0000000000000ull;
        R(state, 11) &= R(state, 12);
        CompareSigned64(cr6, R(state, 11), 0, integer.xer_so);
        R(state, 11) = cr6.eq ? 0u : 1u;
    }
    else R(state, 11) = 1u;
    R(state, 11) = static_cast<std::uint32_t>(R(state, 11)) & 0xffu;
    CompareSigned(integer.cr0, R(state, 11), 0, integer.xer_so);
    if (!integer.cr0.eq) return;
    R(state, 11) = ReadU64(memory, Address(integer.sp + 16u));
    R(state, 11) &= 0x8000000000000000ull;
    WriteU64(memory, Address(integer.sp + 16u), R(state, 11));
    DisableFlush(state, services);
    f1 = ReadU64(memory, Address(integer.sp + 16u));
}

void ClampBinary64(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.classifier.integer;
    auto& f0 = state.classifier.f0_bits;
    auto& f1 = state.classifier.f1_bits;
    EnterFrame(memory, services, state);
    Classify(memory, services, state, 0x83053378u);
    CompareSigned(integer.cr6, R(state, 3), 32, integer.xer_so);
    const bool below_32 = !integer.cr6.gt;
    if (below_32)
    {
        if (integer.cr6.eq) goto low_constant;
        CompareSigned(integer.cr6, R(state, 3), 0, integer.xer_so);
        if (!integer.cr6.gt) goto clamp;
        CompareSigned(integer.cr6, R(state, 3), 2, integer.xer_so);
        if (!integer.cr6.gt) goto quiet_nan;
        CompareSigned(integer.cr6, R(state, 3), 4, integer.xer_so);
        if (integer.cr6.eq) goto low_constant;
        CompareSigned(integer.cr6, R(state, 3), 16, integer.xer_so);
        if (integer.cr6.eq) goto low_constant;
        goto clamp;
    }
    CompareSigned(integer.cr6, R(state, 3), 64, integer.xer_so);
    if (integer.cr6.eq) goto low_constant;
    CompareSigned(integer.cr6, R(state, 3), 128, integer.xer_so);
    if (integer.cr6.eq) goto low_constant;
    CompareSigned(integer.cr6, R(state, 3), 512, integer.xer_so);
    if (integer.cr6.eq) goto high_constant;
    goto clamp;

low_constant:
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2113929216});
    LoadSingle(memory, services, state, R(state, 11) + 3664u, f1);
    goto done;
quiet_nan:
    R(state, 11) = static_cast<std::uint64_t>(std::int64_t{-4194304});
    memory.WriteU32(Address(integer.sp + 80u),
        static_cast<std::uint32_t>(R(state, 11)));
    LoadSingle(memory, services, state, integer.sp + 80u, f1);
    goto done;
high_constant:
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2113929216});
    LoadSingle(memory, services, state, R(state, 11) + 30596u, f1);
    goto done;
clamp:
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2113929216});
    LoadSingle(memory, services, state, R(state, 11) + 3664u, f0);
    CompareFp(integer.cr6, state.f31_bits, f0);
    if (integer.cr6.lt) state.f31_bits = f0;
    else
    {
        R(state, 11) = static_cast<std::uint64_t>(
            std::int64_t{-2113929216});
        LoadSingle(memory, services, state, R(state, 11) + 30596u, f0);
        CompareFp(integer.cr6, state.f31_bits, f0);
        if (integer.cr6.gt) state.f31_bits = f0;
    }
    DisableFlush(state, services);
    f1 = state.f31_bits;
done:
    LeaveFrame(memory, services, state);
}

void SqrtBinary64(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.classifier.integer;
    auto& f0 = state.classifier.f0_bits;
    auto& f1 = state.classifier.f1_bits;
    EnterFrame(memory, services, state);
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2113929216});
    DisableFlush(state, services);
    f1 = ReadU64(memory, Address(R(state, 11) + 3880u));
    CompareFp(integer.cr6, state.f31_bits, f1);
    if (integer.cr6.eq) goto done;
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2113929216});
    DisableFlush(state, services);
    f0 = ReadU64(memory, Address(R(state, 11) + 4072u));
    CompareFp(integer.cr6, state.f31_bits, f0);
    if (integer.cr6.lt) goto quiet_nan;
    DisableFlush(state, services);
    f1 = state.f31_bits;
    Classify(memory, services, state, 0x8305365cu);
    CompareSigned(integer.cr0, R(state, 3), 0, integer.xer_so);
    if (!integer.cr0.gt) goto compute;
    CompareSigned(integer.cr6, R(state, 3), 2, integer.xer_so);
    if (!integer.cr6.gt) goto quiet_nan;
    CompareSigned(integer.cr6, R(state, 3), 4, integer.xer_so);
    if (integer.cr6.eq) goto quiet_nan;
    CompareSigned(integer.cr6, R(state, 3), 512, integer.xer_so);
    if (integer.cr6.eq)
    {
        R(state, 11) = 2139095040u;
        goto from_single;
    }
compute:
    DisableFlush(state, services);
    f1 = std::bit_cast<std::uint64_t>(
        std::sqrt(std::bit_cast<double>(state.f31_bits)));
    integer.lr = 0x8305368cu;
    NormalizeNonzero(memory, services, state);
    goto done;
quiet_nan:
    R(state, 11) = static_cast<std::uint64_t>(std::int64_t{-4194304});
from_single:
    memory.WriteU32(Address(integer.sp + 80u),
        static_cast<std::uint32_t>(R(state, 11)));
    LoadSingle(memory, services, state, integer.sp + 80u, f1);
done:
    LeaveFrame(memory, services, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Services& services,
    Registers& registers)
{
    switch (entry)
    {
    case 0x83053360u: ClampBinary64(memory, services, registers); return true;
    case 0x83053610u: SqrtBinary64(memory, services, registers); return true;
    case 0x82ff99e0u: NormalizeNonzero(memory, services, registers);
        return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_fp_numeric_routes
