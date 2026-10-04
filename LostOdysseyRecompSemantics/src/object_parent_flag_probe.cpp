#include "lo_semantics/object_parent_flag_probe.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>

namespace lo::semantic::gpu::object_parent_flag_probe
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
void CompareSigned(Registers& state, std::uint64_t left,
    std::uint64_t right)
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
void SaveGprs(GuestMemory& memory, Registers& state, unsigned first)
{
    const auto sp = Address(R(state, 1));
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(memory, sp - 16u - (31u - i) * 8u, R(state, i));
    memory.WriteU32(sp - 8u, Address(R(state, 12)));
}
void RestoreGprs(GuestMemory& memory, Registers& state, unsigned first)
{
    const auto sp = Address(R(state, 1));
    for (unsigned i = first; i <= 31u; ++i)
        R(state, i) = ReadU64(memory, sp - 16u - (31u - i) * 8u);
    R(state, 12) = memory.ReadU32(sp - 8u);
    state.lr = R(state, 12);
}
void PushFrame(GuestMemory& memory, Registers& state, std::uint32_t size)
{
    const auto previous = Address(R(state, 1));
    R(state, 1) -= size;
    memory.WriteU32(Address(R(state, 1)), previous);
}
void ResolveRecord(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    object_float_record::Registers record{};
    record.sp = R(state, 1); record.lr = state.lr;
    record.r3 = R(state, 3); record.r11 = R(state, 11);
    record.r12 = R(state, 12); record.r31 = R(state, 31);
    record.xer_so = state.xer_so;
    record.cr6 = {state.cr6.lt, state.cr6.gt,
        state.cr6.eq, state.cr6.un};
    (void)object_float_record_post_chain::Apply(memory,
        dependencies.record_chain, record);
    R(state, 1) = record.sp; state.lr = record.lr;
    R(state, 3) = record.r3; R(state, 11) = record.r11;
    R(state, 12) = record.r12; R(state, 31) = record.r31;
    state.cr6 = {record.cr6.lt, record.cr6.gt,
        record.cr6.eq, record.cr6.un};
}

void Probe(GuestMemory& memory, Dependencies dependencies, Registers& state)
{
    R(state, 12) = state.lr;
    state.lr = 0x82384b28u;
    SaveGprs(memory, state, 22u);
    PushFrame(memory, state, 176u);
    R(state, 23) = R(state, 3);
    R(state, 22) = 0;
    R(state, 11) = memory.ReadU32(Address(R(state, 23) + 120u));
    CompareSigned(state, R(state, 11), 0);
    bool found = false;
    if (state.cr6.gt)
    {
        R(state, 24) = 0;
        R(state, 25) = 0xffffffff80000000ull;
        do
        {
            R(state, 11) = memory.ReadU32(Address(R(state, 23) + 116u));
            R(state, 27) = memory.ReadU32(Address(R(state, 24) + R(state, 11)));
            CompareWord(state, R(state, 27), 0);
            if (!state.cr6.eq)
            {
                R(state, 11) = memory.ReadU32(Address(R(state, 27) + 220u));
                R(state, 26) = 0;
                CompareSigned(state, R(state, 11), 0);
                if (state.cr6.gt)
                {
                    R(state, 28) = 0;
                    do
                    {
                        R(state, 11) = memory.ReadU32(Address(R(state, 27) + 216u));
                        R(state, 30) = memory.ReadU32(Address(R(state, 28) + R(state, 11)));
                        CompareWord(state, R(state, 30), 0);
                        if (!state.cr6.eq)
                        {
                            R(state, 11) = memory.ReadU32(Address(R(state, 30) + 80u));
                            R(state, 29) = 0;
                            CompareSigned(state, R(state, 11), 0);
                            if (state.cr6.gt)
                            {
                                R(state, 31) = 0;
                                do
                                {
                                    R(state, 11) = memory.ReadU32(Address(R(state, 30) + 76u));
                                    R(state, 3) = memory.ReadU32(Address(R(state, 31) + R(state, 11)));
                                    state.lr = 0x82384b9cu;
                                    ResolveRecord(memory, dependencies, state);
                                    CompareWord(state, R(state, 3), 0);
                                    if (!state.cr6.eq)
                                    {
                                        R(state, 11) = memory.ReadU32(Address(R(state, 3) + 96u));
                                        R(state, 11) &= 0x80000000u;
                                        CompareWord(state, R(state, 11), R(state, 25));
                                        if (state.cr6.eq)
                                        {
                                            found = true;
                                            break;
                                        }
                                    }
                                    R(state, 11) = memory.ReadU32(Address(R(state, 30) + 80u));
                                    R(state, 29) += 1u;
                                    R(state, 31) += 4u;
                                    CompareSigned(state, R(state, 29), R(state, 11));
                                } while (state.cr6.lt);
                            }
                        }
                        if (found) break;
                        R(state, 11) = memory.ReadU32(Address(R(state, 27) + 220u));
                        R(state, 26) += 1u;
                        R(state, 28) += 4u;
                        CompareSigned(state, R(state, 26), R(state, 11));
                    } while (state.cr6.lt);
                }
            }
            if (found) break;
            R(state, 11) = memory.ReadU32(Address(R(state, 23) + 120u));
            R(state, 22) += 1u;
            R(state, 24) += 4u;
            CompareSigned(state, R(state, 22), R(state, 11));
        } while (state.cr6.lt);
    }
    R(state, 3) = found ? 1u : 0u;
    R(state, 1) += 176u;
    RestoreGprs(memory, state, 22u);
}

void EvaluateOwner(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    R(state, 12) = state.lr;
    memory.WriteU32(Address(R(state, 1) - 8u), Address(R(state, 12)));
    WriteU64(memory, Address(R(state, 1) - 24u), R(state, 30));
    WriteU64(memory, Address(R(state, 1) - 16u), R(state, 31));
    PushFrame(memory, state, 112u);
    R(state, 31) = R(state, 3);
    R(state, 3) = memory.ReadU32(Address(R(state, 31) + 124u));
    R(state, 11) = memory.ReadU32(Address(R(state, 31) + 108u));
    CompareWord(state, R(state, 3), 0);
    R(state, 30) = (Address(R(state, 11)) >> 31) & 1u;
    if (!state.cr6.eq)
    {
        R(state, 5) = 1;
        R(state, 4) = 0;
        state.lr = 0x822c5df8u;
        (void)object_child_float_record_chain::Apply(0x822c5e58u,
            memory, dependencies.child_chain, state);

        R(state, 11) = 0xffffffff82000000ull;
        constexpr std::uint32_t FlushMask = 0x8040u;
        if (state.cached_fp_control & FlushMask)
        {
            state.cached_fp_control &= ~FlushMask;
            dependencies.dynamic.SetHostFpControl(state.cached_fp_control);
        }
        state.f0_bits = LoadedSingle::FromWord(
            memory.ReadU32(Address(R(state, 11) + 3664u))).FprBits();
        CompareFloat(state, state.f1_bits, state.f0_bits);
        if (state.cr6.lt)
            R(state, 30) = 1;
        else
        {
            R(state, 3) = memory.ReadU32(Address(R(state, 31) + 124u));
            state.lr = 0x822c5e10u;
            Probe(memory, dependencies, state);
            CompareSigned(state, R(state, 3), 0);
            if (!state.cr6.eq)
                R(state, 30) = 1;
        }

        R(state, 11) = memory.ReadU32(Address(R(state, 31)));
        R(state, 3) = R(state, 31);
        R(state, 11) = memory.ReadU32(Address(R(state, 11) + 376u));
        state.ctr = R(state, 11);
        state.lr = 0x822c5e30u;
        dependencies.dynamic.CallGuest(Address(state.ctr) & ~GuestAddress{3},
            memory, state);
        CompareSigned(state, R(state, 3), 0);
        R(state, 3) = state.cr6.eq ? R(state, 30) : 0;
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
    case 0x82384b20u: Probe(memory, dependencies, state); return true;
    case 0x822c5dc0u: EvaluateOwner(memory, dependencies, state); return true;
    default: return false;
    }
}

} // namespace lo::semantic::gpu::object_parent_flag_probe
