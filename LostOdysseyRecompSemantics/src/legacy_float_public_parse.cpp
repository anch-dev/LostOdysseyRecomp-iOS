#include "lo_semantics/legacy_float_public_parse.h"

#include "lo_semantics/legacy_token_lookup_flags.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/registered_metadata_string.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::legacy_float_public_parse
{
namespace
{
namespace route = legacy_float_parse_routes;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& state, unsigned index)
{ return state.route.integer.r[index]; }

void CompareUnsigned(crt_stream_operations::Registers& state,
    crt_stream_operations::Condition& condition,
    std::uint64_t left, std::uint32_t right)
{
    const auto value = static_cast<std::uint32_t>(left);
    condition = {std::uint8_t(value < right),
        std::uint8_t(value > right), std::uint8_t(value == right),
        state.xer_so};
}

void CompareCr0Signed(Registers& state, std::uint64_t value)
{
    const auto word = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(value));
    auto& integer = state.route.integer;
    integer.cr0 = {std::uint8_t(word < 0),
        std::uint8_t(word > 0), std::uint8_t(word == 0), integer.xer_so};
}

void DisableFlush(Registers& state, NativeServices& services)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if (state.route.cached_fp_control & FlushMask)
    {
        state.route.cached_fp_control &= ~FlushMask;
        services.SetHostFpControl(state.route.cached_fp_control);
    }
}

void SaveFrame(GuestMemory& memory, NativeServices& services,
    Registers& state)
{
    auto& integer = state.route.integer;
    const auto sp = integer.sp;
    R(state, 12) = integer.lr;
    for (unsigned index = 28u; index <= 31u; ++index)
        WriteU64(memory, Address(sp - 40u + (index - 28u) * 8u),
            R(state, index));
    memory.WriteU32(Address(sp - 8u), Address(R(state, 12)));
    DisableFlush(state, services);
    WriteU64(memory, Address(sp - 48u), state.f31_bits);
    memory.WriteU32(Address(sp - 160u), Address(sp));
    integer.sp -= 160u;
}

void RestoreFrame(GuestMemory& memory, NativeServices& services,
    Registers& state)
{
    auto& integer = state.route.integer;
    integer.sp += 160u;
    const auto sp = integer.sp;
    DisableFlush(state, services);
    state.f31_bits = ReadU64(memory, Address(sp - 48u));
    for (unsigned index = 28u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory,
            Address(sp - 40u + (index - 28u) * 8u));
    R(state, 12) = memory.ReadU32(Address(sp - 8u));
    integer.lr = R(state, 12);
}

class RouteServices final : public route::PpcBoundaryServices
{
public:
    explicit RouteServices(NativeServices& native, Registers& outer)
        : native_(native), outer_(outer) {}

    void Parse822975B0(GuestMemory& memory,
        route::Registers& state) override
    {
        outer_.route = state;
        native_.Parse822975B0(memory, outer_);
        state = outer_.route;
    }

    void Classify822981C8(GuestMemory&, route::Registers&) override
    { throw std::logic_error("822981C8 must use its recovered lower"); }

    void InvalidArgument82B7FD78(GuestMemory& memory,
        route::Registers& state) override
    {
        outer_.route = state;
        native_.InvalidArgument82B7FD78(memory, outer_);
        state = outer_.route;
    }

    void InvalidParameter82B7FEC0(GuestMemory& memory,
        route::Registers& state) override
    {
        outer_.route = state;
        native_.InvalidParameter82B7FEC0(memory, outer_);
        state = outer_.route;
    }

    void SetHostFpControl(std::uint32_t control) override
    { native_.SetHostFpControl(control); }

private:
    NativeServices& native_;
    Registers& outer_;
};

void Parse(GuestMemory& memory, NativeServices& services,
    Registers& state)
{
    auto& integer = state.route.integer;
    SaveFrame(memory, services, state);
    R(state, 28) = R(state, 3);
    R(state, 29) = R(state, 4);
    R(state, 31) = R(state, 28);
    CompareUnsigned(integer, integer.cr6, R(state, 29), 0u);
    if (!integer.cr6.eq)
        memory.WriteU32(Address(R(state, 29)), Address(R(state, 28)));

    CompareUnsigned(integer, integer.cr6, R(state, 28), 0u);
    if (integer.cr6.eq)
    {
        integer.lr = 0x82b7e0ccu;
        services.InvalidArgument82B7FD78(memory, state);
        R(state, 11) = R(state, 3);
        R(state, 10) = 22u;
        for (unsigned index = 3u; index <= 7u; ++index)
            R(state, index) = 0u;
        memory.WriteU32(Address(R(state, 11)), 22u);
        integer.lr = 0x82b7e0f0u;
        services.InvalidParameter82B7FEC0(memory, state);
        R(state, 11) = static_cast<std::uint64_t>(
            std::int64_t{-2113929216});
        DisableFlush(state, services);
        state.route.f1_bits = ReadU64(memory, Address(R(state, 11) + 4072u));
        RestoreFrame(memory, services, state);
        return;
    }

    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2094989312});
    R(state, 3) = memory.ReadU16(Address(R(state, 28)));
    R(state, 30) = R(state, 11) + 21248u;
    for (;;)
    {
        R(state, 5) = R(state, 30);
        R(state, 4) = 8u;
        integer.lr = 0x82b7e120u;
        (void)legacy_token_lookup_flags::Apply(0x822974b0u,
            memory, integer);
        CompareCr0Signed(state, R(state, 3));
        if (integer.cr0.eq) break;
        R(state, 31) += 2u;
        R(state, 3) = memory.ReadU16(Address(R(state, 31)));
    }

    R(state, 3) = R(state, 31);
    integer.lr = 0x82b7e130u;
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
    integer.lr = 0x82b7e14cu;
    RouteServices route_services(services, state);
    (void)route::Apply(0x822974f8u, memory, route_services,
        state.route);

    CompareUnsigned(integer, integer.cr6, R(state, 29), 0u);
    if (!integer.cr6.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 3) + 4u));
        R(state, 11) = WordRotateMask(R(state, 11), 1, 0xfffffffeu);
        R(state, 11) += R(state, 31);
        memory.WriteU32(Address(R(state, 29)), Address(R(state, 11)));
    }

    R(state, 11) = memory.ReadU32(Address(R(state, 3)));
    R(state, 10) = R(state, 11) & 576u;
    CompareUnsigned(integer, integer.cr0, R(state, 10), 0u);
    if (!integer.cr0.eq)
    {
        R(state, 11) = static_cast<std::uint64_t>(
            std::int64_t{-2113929216});
        CompareUnsigned(integer, integer.cr6, R(state, 29), 0u);
        DisableFlush(state, services);
        state.f31_bits = ReadU64(memory, Address(R(state, 11) + 4072u));
        if (!integer.cr6.eq)
            memory.WriteU32(Address(R(state, 29)), Address(R(state, 28)));
    }
    else
    {
        R(state, 10) = R(state, 11) & 129u;
        CompareUnsigned(integer, integer.cr0, R(state, 10), 0u);
        if (!integer.cr0.eq)
        {
            R(state, 11) = memory.ReadU16(Address(R(state, 31)));
            CompareUnsigned(integer, integer.cr6, R(state, 11), 45u);
            R(state, 11) = static_cast<std::uint64_t>(
                std::int64_t{-2113077248});
            if (integer.cr6.eq)
            {
                DisableFlush(state, services);
                state.f0_bits = ReadU64(memory, Address(R(state, 11) + 15088u));
                state.f31_bits = state.f0_bits ^ 0x8000000000000000ull;
            }
            else
            {
                DisableFlush(state, services);
                state.f31_bits = ReadU64(memory, Address(R(state, 11) + 15088u));
            }
            integer.lr = 0x82b7e1dcu;
            services.InvalidArgument82B7FD78(memory, state);
            R(state, 11) = 34u;
            memory.WriteU32(Address(R(state, 3)), 34u);
        }
        else
        {
            R(state, 11) = WordRotateMask(R(state, 11), 0, 0x100u);
            CompareCr0Signed(state, R(state, 11));
            bool error = false;
            if (!integer.cr0.eq)
            {
                R(state, 11) = static_cast<std::uint64_t>(
                    std::int64_t{-2113929216});
                DisableFlush(state, services);
                state.f0_bits = ReadU64(memory, Address(R(state, 3) + 16u));
                state.f31_bits = ReadU64(memory, Address(R(state, 11) + 4072u));
                const double left = std::bit_cast<double>(state.f0_bits);
                const double right = std::bit_cast<double>(state.f31_bits);
                const bool unordered = std::isnan(left) || std::isnan(right);
                integer.cr6 = {std::uint8_t(!unordered && left < right),
                    std::uint8_t(!unordered && left > right),
                    std::uint8_t(!unordered && left == right),
                    std::uint8_t(unordered)};
                error = integer.cr6.eq;
            }
            if (error)
            {
                integer.lr = 0x82b7e1dcu;
                services.InvalidArgument82B7FD78(memory, state);
                R(state, 11) = 34u;
                memory.WriteU32(Address(R(state, 3)), 34u);
            }
            else
            {
                DisableFlush(state, services);
                state.f31_bits = ReadU64(memory, Address(R(state, 3) + 16u));
            }
        }
    }

    DisableFlush(state, services);
    state.route.f1_bits = state.f31_bits;
    RestoreFrame(memory, services, state);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& services, Registers& registers)
{
    switch (entry)
    {
    case 0x82b7e200u: R(registers, 5) = 0u; [[fallthrough]];
    case 0x82b7e098u: Parse(memory, services, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_float_public_parse
