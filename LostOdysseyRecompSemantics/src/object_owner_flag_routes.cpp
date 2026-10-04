#include "lo_semantics/object_owner_flag_routes.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <stdexcept>

namespace lo::semantic::gpu::object_owner_flag_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Registers = object_child_float::Registers;

std::uint64_t& R(Registers& state, unsigned index) { return state.r[index]; }

void CompareWord(Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = Address(left), b = Address(right);
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}
void CompareSigned(Registers& state, std::uint64_t left, std::uint64_t right)
{
    const auto a = std::bit_cast<std::int32_t>(Address(left));
    const auto b = std::bit_cast<std::int32_t>(Address(right));
    state.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), state.xer_so};
}
void CompareFloat(Registers& state, std::uint64_t left_bits,
    std::uint64_t right_bits)
{
    const auto a = std::bit_cast<double>(left_bits);
    const auto b = std::bit_cast<double>(right_bits);
    const bool un = std::isnan(a) || std::isnan(b);
    state.cr6 = {std::uint8_t(!un && a < b),
        std::uint8_t(!un && a > b), std::uint8_t(!un && a == b),
        std::uint8_t(un)};
}
void DisableFlush(Dependencies dependencies, Registers& state)
{
    constexpr std::uint32_t FlushMask = 0x8040u;
    if (state.cached_fp_control & FlushMask)
    {
        state.cached_fp_control &= ~FlushMask;
        dependencies.probe.dynamic.SetHostFpControl(state.cached_fp_control);
    }
}
void LoadSingle(GuestMemory& memory, Dependencies dependencies,
    Registers& state, std::uint32_t offset, std::uint64_t& target)
{
    DisableFlush(dependencies, state);
    target = LoadedSingle::FromWord(memory.ReadU32(0x82000000u + offset))
        .FprBits();
}

void EvaluateSelector(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 11) = 0xffffffff82000000ull;
    CompareSigned(state, R(state, 4), 0);
    LoadSingle(memory, dependencies, state, 3664u, state.f1_bits);
    if (state.cr6.eq)
    {
        R(state, 11) = memory.ReadU32(Address(R(state, 3) + 64u));
        R(state, 11) = memory.ReadU32(Address(R(state, 11) + 60u));
        R(state, 11) &= 0x80000000u;
        CompareWord(state, R(state, 11), 0);
        if (!state.cr6.eq)
        {
            R(state, 11) = 0xffffffff82000000ull;
            LoadSingle(memory, dependencies, state, 3648u, state.f1_bits);
            return;
        }
    }
    R(state, 3) = memory.ReadU32(Address(R(state, 3) + 60u));
    CompareWord(state, R(state, 3), 0);
    if (!state.cr6.eq &&
        !object_child_float_record_chain::Apply(0x822c5e58u,
            memory, dependencies.probe.child_chain, state))
        throw std::runtime_error("missing 822C5E58 owner route");
}

void EvaluateOwner(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 24u), R(state, 30));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    const auto previous_sp = Address(R(state, 1));
    R(state, 1) -= 112u;
    memory.WriteU32(Address(R(state, 1)), previous_sp);
    R(state, 31) = R(state, 3);
    R(state, 3) = memory.ReadU32(Address(R(state, 31) + 128u));
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 108u));
    CompareWord(state, R(state, 3), 0);
    R(state, 30) = (Address(R(state, 11)) >> 31) & 1u;
    if (!state.cr6.eq)
    {
        R(state, 5) = 1u;
        R(state, 4) = 0u;
        state.lr = 0x8236b508u;
        EvaluateSelector(memory, dependencies, state);

        R(state, 11) = 0xffffffff82000000ull;
        LoadSingle(memory, dependencies, state, 3664u, state.f0_bits);
        CompareFloat(state, state.f1_bits, state.f0_bits);
        if (state.cr6.lt)
            R(state, 30) = 1u;
        else
        {
            R(state, 11) = memory.ReadU32(Address(R(state, 31) + 128u));
            R(state, 3) = memory.ReadU32(Address(R(state, 11) + 60u));
            CompareWord(state, R(state, 3), 0);
            if (!state.cr6.eq)
            {
                state.lr = 0x8236b52cu;
                if (!object_parent_flag_probe::Apply(0x82384b20u,
                        memory, dependencies.probe, state))
                    throw std::runtime_error("missing 82384B20 owner probe");
                CompareSigned(state, R(state, 3), 0);
                if (!state.cr6.eq) R(state, 30) = 1u;
            }
        }
        R(state, 11) = memory.ReadU32(Address(R(state, 31)));
        R(state, 3) = R(state, 31);
        R(state, 11) = memory.ReadU32(Address(R(state, 11) + 376u));
        state.ctr = R(state, 11);
        state.lr = 0x8236b54cu;
        dependencies.probe.dynamic.CallGuest(
            Address(state.ctr) & ~GuestAddress{3}, memory, state);
        CompareSigned(state, R(state, 3), 0);
        R(state, 3) = state.cr6.eq ? R(state, 30) : 0u;
    }
    else
        R(state, 3) = R(state, 30);

    R(state, 1) += 112u;
    R(state, 12) = memory.ReadU32(Address(R(state, 1) - 8u));
    state.lr = R(state, 12);
    R(state, 30) = ReadU64(memory, Address(R(state, 1) - 24u));
    R(state, 31) = ReadU64(memory, Address(R(state, 1) - 16u));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state)
{
    switch (entry)
    {
    case 0x8236b578u: EvaluateSelector(memory, dependencies, state); return true;
    case 0x8236b4d0u: EvaluateOwner(memory, dependencies, state); return true;
    default: return false;
    }
}

} // namespace lo::semantic::gpu::object_owner_flag_routes
