#include "lo_semantics/legacy_fp_flagged_routes.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_fp_flagged_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Services = legacy_fp_classification::HostFpServices;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Registers& state, unsigned index)
{
    return index == 1u ? state.numeric.classifier.integer.sp :
        state.numeric.classifier.integer.r[index];
}

void DisableFlush(Registers& state, Services& services)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    auto& control = state.numeric.classifier.cached_fp_control;
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

void TestWord(Registers& state, std::uint64_t value)
{
    auto& integer = state.numeric.classifier.integer;
    CompareSigned(integer.cr0, value, 0, integer.xer_so);
}

void CopySign(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.numeric.classifier.integer;
    auto& fp = state.numeric.classifier;
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2113929216});
    DisableFlush(state, services);
    WriteU64(memory, Address(integer.sp + 16u), fp.f1_bits);
    WriteU64(memory, Address(integer.sp + 24u), state.f2_bits);
    fp.f0_bits = ReadU64(memory, Address(R(state, 11) + 4072u));
    WriteU64(memory, Address(integer.sp - 16u), fp.f0_bits);
    R(state, 10) = memory.ReadU32(Address(integer.sp + 24u));
    R(state, 11) = memory.ReadU32(Address(integer.sp + 16u));
    R(state, 9) = memory.ReadU32(Address(integer.sp + 20u));
    R(state, 10) = (WordRotateMask(R(state, 11), 0, 0x7fffffffu) |
        (R(state, 10) & 0xffffffff80000000ull));
    memory.WriteU32(Address(integer.sp - 12u),
        static_cast<std::uint32_t>(R(state, 9)));
    memory.WriteU32(Address(integer.sp - 16u),
        static_cast<std::uint32_t>(R(state, 10)));
    fp.f1_bits = ReadU64(memory, Address(integer.sp - 16u));
}

void FlipSign(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.numeric.classifier.integer;
    auto& fp = state.numeric.classifier;
    DisableFlush(state, services);
    WriteU64(memory, Address(integer.sp + 16u), fp.f1_bits);
    R(state, 10) = static_cast<std::uint64_t>(
        std::int64_t{-2113929216});
    fp.f0_bits = ReadU64(memory, Address(R(state, 10) + 4072u));
    R(state, 11) = memory.ReadU32(Address(integer.sp + 16u));
    WriteU64(memory, Address(integer.sp - 16u), fp.f0_bits);
    R(state, 9) = ~R(state, 11);
    R(state, 9) = (WordRotateMask(R(state, 11), 0, 0x7fffffffu) |
        (R(state, 9) & 0xffffffff80000000ull));
    memory.WriteU32(Address(integer.sp - 16u),
        static_cast<std::uint32_t>(R(state, 9)));
    R(state, 10) = memory.ReadU32(Address(integer.sp + 20u));
    memory.WriteU32(Address(integer.sp - 12u),
        static_cast<std::uint32_t>(R(state, 10)));
    fp.f1_bits = ReadU64(memory, Address(integer.sp - 16u));
}

void EnterFrame(GuestMemory& memory, Registers& state, bool second)
{
    auto& integer = state.numeric.classifier.integer;
    R(state, 12) = integer.lr;
    memory.WriteU32(Address(integer.sp - 8u),
        static_cast<std::uint32_t>(R(state, 12)));
    if (second)
        WriteU64(memory, Address(integer.sp - 24u), R(state, 30));
    WriteU64(memory, Address(integer.sp - 16u), R(state, 31));
    memory.WriteU32(Address(integer.sp - 112u),
        static_cast<std::uint32_t>(integer.sp));
    integer.sp -= 112u;
}

void LeaveFrame(GuestMemory& memory, Registers& state, bool second)
{
    auto& integer = state.numeric.classifier.integer;
    integer.sp += 112u;
    R(state, 12) = memory.ReadU32(Address(integer.sp - 8u));
    integer.lr = R(state, 12);
    if (second) R(state, 30) = ReadU64(memory, Address(integer.sp - 24u));
    R(state, 31) = ReadU64(memory, Address(integer.sp - 16u));
}

void Clamp(GuestMemory& memory, Services& services,
    Registers& state, GuestAddress return_address)
{
    state.numeric.classifier.integer.lr = return_address;
    (void)legacy_fp_numeric_routes::Apply(0x83053360u, memory,
        services, state.numeric);
}

void RoundSingle(Services& services, Registers& state)
{
    DisableFlush(state, services);
    auto& f1 = state.numeric.classifier.f1_bits;
    f1 = std::bit_cast<std::uint64_t>(
        static_cast<double>(static_cast<float>(std::bit_cast<double>(f1))));
}

void FlaggedRoundSign(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.numeric.classifier.integer;
    auto& fp = state.numeric.classifier;
    EnterFrame(memory, state, false);
    R(state, 31) = R(state, 4);
    R(state, 11) = static_cast<std::uint32_t>(R(state, 31)) & 1u;
    TestWord(state, R(state, 11));
    if (!integer.cr0.eq) Clamp(memory, services, state, 0x83053c30u);
    R(state, 11) = WordRotateMask(R(state, 31), 0, 2u);
    TestWord(state, R(state, 11));
    if (!integer.cr0.eq)
    {
        R(state, 11) = static_cast<std::uint64_t>(
            std::int64_t{-2113929216});
        DisableFlush(state, services);
        state.f2_bits = ReadU64(memory, Address(R(state, 11) + 4072u));
        integer.lr = 0x83053c44u;
        CopySign(memory, services, state);
        RoundSingle(services, state);
    }
    R(state, 11) = WordRotateMask(R(state, 31), 0, 4u);
    TestWord(state, R(state, 11));
    if (!integer.cr0.eq)
    {
        DisableFlush(state, services);
        const auto single = std::bit_cast<std::uint32_t>(
            static_cast<float>(std::bit_cast<double>(fp.f1_bits)));
        memory.WriteU32(Address(integer.sp + 80u), single);
        R(state, 11) = memory.ReadU32(Address(integer.sp + 80u));
        R(state, 11) ^= 0x80000000u;
        memory.WriteU32(Address(integer.sp + 132u),
            static_cast<std::uint32_t>(R(state, 11)));
        DisableFlush(state, services);
        fp.f1_bits = std::bit_cast<std::uint64_t>(
            static_cast<double>(std::bit_cast<float>(
                memory.ReadU32(Address(integer.sp + 132u)))));
    }
    LeaveFrame(memory, state, false);
}

void FlaggedFormatSign(GuestMemory& memory, Services& services,
    Registers& state)
{
    auto& integer = state.numeric.classifier.integer;
    auto& fp = state.numeric.classifier;
    EnterFrame(memory, state, true);
    R(state, 31) = R(state, 5);
    R(state, 30) = R(state, 4);
    R(state, 11) = static_cast<std::uint32_t>(R(state, 31)) & 1u;
    TestWord(state, R(state, 11));
    if (!integer.cr0.eq)
    {
        RoundSingle(services, state);
        Clamp(memory, services, state, 0x83053ca4u);
    }
    R(state, 11) = WordRotateMask(R(state, 31), 0, 2u);
    TestWord(state, R(state, 11));
    if (!integer.cr0.eq)
    {
        R(state, 11) = static_cast<std::uint64_t>(
            std::int64_t{-2113929216});
        DisableFlush(state, services);
        state.f2_bits = ReadU64(memory, Address(R(state, 11) + 4072u));
        integer.lr = 0x83053cb8u;
        CopySign(memory, services, state);
    }
    R(state, 11) = WordRotateMask(R(state, 31), 0, 4u);
    TestWord(state, R(state, 11));
    if (!integer.cr0.eq)
    {
        CompareSigned(integer.cr6, R(state, 30), 2, integer.xer_so);
        if (integer.cr6.eq)
        {
            R(state, 11) = static_cast<std::uint64_t>(
                std::int64_t{-2112815104});
            DisableFlush(state, services);
            fp.f0_bits = ReadU64(memory, Address(R(state, 11) - 21816u));
            fp.f1_bits = std::bit_cast<std::uint64_t>(
                std::bit_cast<double>(fp.f0_bits) -
                std::bit_cast<double>(fp.f1_bits));
        }
        else
        {
            integer.lr = 0x83053cdcu;
            FlipSign(memory, services, state);
        }
    }
    LeaveFrame(memory, state, true);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory, Services& services,
    Registers& registers)
{
    switch (entry)
    {
    case 0x83053c10u: FlaggedRoundSign(memory, services, registers);
        return true;
    case 0x83053c78u: FlaggedFormatSign(memory, services, registers);
        return true;
    case 0x82b7def0u: CopySign(memory, services, registers); return true;
    case 0x82b7df28u: FlipSign(memory, services, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_fp_flagged_routes
