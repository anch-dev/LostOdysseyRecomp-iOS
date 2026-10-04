#include "lo_semantics/legacy_float_parse_routes.h"

#include "lo_semantics/legacy_float_binary_conversion.h"
#include "lo_semantics/legacy_token_lookup_flags.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/registered_metadata_string.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_float_parse_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
namespace binary = legacy_float_binary_conversion;

binary::Registers ToBinary(const Registers& state)
{
    binary::Registers result{};
    result.r = state.integer.r;
    result.r[1] = state.integer.sp;
    result.ctr = state.integer.ctr;
    result.lr = state.integer.lr;
    result.xer.so = state.integer.xer_so;
    result.xer.ca = state.integer.xer_ca;
    result.cr0 = {state.integer.cr0.lt, state.integer.cr0.gt,
        state.integer.cr0.eq, state.integer.cr0.so};
    result.cr6 = {state.integer.cr6.lt, state.integer.cr6.gt,
        state.integer.cr6.eq, state.integer.cr6.so};
    return result;
}
void FromBinary(Registers& state, const binary::Registers& result)
{
    const auto unexposed_r1 = state.integer.r[1];
    state.integer.r = result.r;
    state.integer.r[1] = unexposed_r1;
    state.integer.sp = result.r[1];
    state.integer.ctr = result.ctr;
    state.integer.lr = result.lr;
    state.integer.xer_so = result.xer.so;
    state.integer.xer_ca = result.xer.ca;
    state.integer.cr0 = {result.cr0.lt, result.cr0.gt,
        result.cr0.eq, result.cr0.so};
    state.integer.cr6 = {result.cr6.lt, result.cr6.gt,
        result.cr6.eq, result.cr6.so};
}

std::uint64_t& R(Registers& state, unsigned index)
{ return state.integer.r[index]; }

std::int32_t SignedWord(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)); }

void CompareSigned(Registers& state, std::uint64_t value,
    std::int32_t right)
{
    const auto left = SignedWord(value);
    state.integer.cr6 = {std::uint8_t(left < right),
        std::uint8_t(left > right), std::uint8_t(left == right),
        state.integer.xer_so};
}

void CompareCr0Zero(Registers& state, std::uint64_t value)
{
    const auto word = SignedWord(value);
    state.integer.cr0 = {std::uint8_t(word < 0),
        std::uint8_t(word > 0), std::uint8_t(word == 0),
        state.integer.xer_so};
}

void DisableFlush(Registers& state, PpcBoundaryServices& services)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if (state.cached_fp_control & FlushMask)
    {
        state.cached_fp_control &= ~FlushMask;
        services.SetHostFpControl(state.cached_fp_control);
    }
}

void SaveFrame(GuestMemory& memory, Registers& state,
    unsigned first, std::uint64_t size)
{
    const auto sp = state.integer.sp;
    R(state, 12) = state.integer.lr;
    for (unsigned index = first; index <= 31u; ++index)
        WriteU64(memory, Address(sp - 8u * (33u - index)),
            R(state, index));
    memory.WriteU32(Address(sp - 8u), Address(R(state, 12)));
    memory.WriteU32(Address(sp - size), Address(sp));
    state.integer.sp -= size;
}

void RestoreFrame(GuestMemory& memory, Registers& state,
    unsigned first, std::uint64_t size)
{
    state.integer.sp += size;
    const auto sp = state.integer.sp;
    R(state, 12) = memory.ReadU32(Address(sp - 8u));
    state.integer.lr = R(state, 12);
    for (unsigned index = first; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(sp - 8u * (33u - index)));
}

void Route(GuestMemory& memory, PpcBoundaryServices& services,
    Registers& state)
{
    auto& integer = state.integer;
    SaveFrame(memory, state, 27u, 160u);
    R(state, 28) = R(state, 4);
    R(state, 29) = R(state, 3);
    R(state, 10) = R(state, 8);
    R(state, 27) = 0u;
    for (unsigned index = 6u; index <= 9u; ++index)
        R(state, index) = 0u;
    R(state, 5) = R(state, 28);
    R(state, 4) = integer.sp + 80u;
    R(state, 3) = integer.sp + 96u;
    R(state, 30) = R(state, 27);
    integer.lr = 0x82297538u;
    services.Parse822975B0(memory, state);
    R(state, 31) = R(state, 3);
    R(state, 11) = R(state, 31) & 4u;
    CompareCr0Zero(state, R(state, 11));
    if (!integer.cr0.eq)
    {
        R(state, 30) = 512u;
        memory.WriteU32(Address(integer.sp + 88u), 0u);
        memory.WriteU32(Address(integer.sp + 92u), 0u);
    }
    else
    {
        R(state, 4) = integer.sp + 88u;
        R(state, 3) = integer.sp + 96u;
        integer.lr = 0x82297560u;
        auto converted = ToBinary(state);
        (void)binary::Apply(0x822981c8u, memory, converted);
        FromBinary(state, converted);
        R(state, 11) = R(state, 31) & 2u;
        CompareCr0Zero(state, R(state, 11));
        if (!integer.cr0.eq)
            R(state, 30) = 128u;
        else
        {
            CompareSigned(state, R(state, 3), 1);
            if (integer.cr6.eq) R(state, 30) = 128u;
        }
        R(state, 11) = R(state, 31) & 1u;
        CompareCr0Zero(state, R(state, 11));
        if (!integer.cr0.eq)
            R(state, 30) |= 256u;
        else
        {
            CompareSigned(state, R(state, 3), 2);
            if (integer.cr6.eq) R(state, 30) |= 256u;
        }
    }
    R(state, 11) = memory.ReadU32(Address(integer.sp + 80u));
    R(state, 3) = R(state, 29);
    R(state, 10) = ReadU64(memory, Address(integer.sp + 88u));
    R(state, 11) -= R(state, 28);
    const auto low = static_cast<std::uint32_t>(R(state, 11));
    integer.xer_ca = std::uint8_t(SignedWord(R(state, 11)) < 0 &&
        (low & 1u) != 0u);
    R(state, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(SignedWord(R(state, 11)) >> 1));
    memory.WriteU32(Address(R(state, 29)),
        static_cast<std::uint32_t>(R(state, 30)));
    WriteU64(memory, Address(R(state, 29) + 16u), R(state, 10));
    memory.WriteU32(Address(R(state, 29) + 4u),
        static_cast<std::uint32_t>(R(state, 11)));
    RestoreFrame(memory, state, 27u, 160u);
}

void InvalidNull(GuestMemory& memory, PpcBoundaryServices& services,
    Registers& state)
{
    state.integer.lr = 0x82b7d294u;
    services.InvalidArgument82B7FD78(memory, state);
    R(state, 11) = R(state, 3);
    R(state, 10) = 22u;
    for (unsigned index = 3u; index <= 7u; ++index)
        R(state, index) = 0u;
    memory.WriteU32(Address(R(state, 11)), 22u);
    state.integer.lr = 0x82b7d2b8u;
    services.InvalidParameter82B7FEC0(memory, state);
    R(state, 11) = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(-2113929216));
    DisableFlush(state, services);
    state.f1_bits = ReadU64(memory, Address(R(state, 11) + 4072u));
}

void ParseDouble(GuestMemory& memory, PpcBoundaryServices& services,
    Registers& state)
{
    auto& integer = state.integer;
    SaveFrame(memory, state, 30u, 128u);
    R(state, 31) = R(state, 3);
    const auto pointer = static_cast<std::uint32_t>(R(state, 31));
    integer.cr6 = {0, std::uint8_t(pointer != 0u),
        std::uint8_t(pointer == 0u), integer.xer_so};
    if (integer.cr6.eq)
        InvalidNull(memory, services, state);
    else
    {
        R(state, 11) = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(-2094989312));
        R(state, 30) = R(state, 11) + 21248u;
        for (;;)
        {
            R(state, 5) = R(state, 30);
            R(state, 3) = memory.ReadU16(Address(R(state, 31)));
            R(state, 4) = 8u;
            integer.lr = 0x82b7d2e4u;
            (void)legacy_token_lookup_flags::Apply(0x822974b0u,
                memory, integer);
            CompareCr0Zero(state, R(state, 3));
            if (integer.cr0.eq) break;
            R(state, 31) += 2u;
        }
        R(state, 3) = R(state, 31);
        integer.lr = 0x82b7d2f4u;
        const auto length = registered_metadata_string::Utf16Length(
            memory, R(state, 3));
        R(state, 10) = 0u;
        R(state, 11) = length + 1u;
        R(state, 3) = length;
        integer.cr0 = {0, 0, 1, integer.xer_so};
        integer.xer_ca = 0u;
        R(state, 5) = R(state, 3);
        R(state, 4) = R(state, 31);
        R(state, 3) = integer.sp + 80u;
        R(state, 6) = 0u;
        R(state, 7) = 0u;
        R(state, 8) = R(state, 30);
        integer.lr = 0x82b7d310u;
        Route(memory, services, state);
        DisableFlush(state, services);
        state.f1_bits = ReadU64(memory, Address(R(state, 3) + 16u));
    }
    RestoreFrame(memory, state, 30u, 128u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    PpcBoundaryServices& services, Registers& registers)
{
    switch (entry)
    {
    case 0x82b7d270u: ParseDouble(memory, services, registers); return true;
    case 0x822974f8u: Route(memory, services, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_float_parse_routes
