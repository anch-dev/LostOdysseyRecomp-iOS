#include "lo_semantics/legacy_descriptor_array_lookup.h"

#include "lo_semantics/legacy_descriptor_record_routes.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cstdint>

namespace lo::semantic::gpu::legacy_descriptor_array_lookup
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

void ReadWord(GuestMemory& memory, Integer& state, unsigned target,
    std::uint64_t address)
{ R(state, target) = memory.ReadU32(Address(address)); }

void WriteWord(GuestMemory& memory, std::uint64_t address,
    std::uint64_t value)
{ memory.WriteU32(Address(address), static_cast<std::uint32_t>(value)); }

void FreeHeapGeneral(GuestMemory& memory, Dependencies dependencies,
    Integer& state)
{
    R(state, 12) = state.lr;
    WriteWord(memory, state.sp - 8u, R(state, 12));
    WriteU64(memory, Address(state.sp - 16u), R(state, 31));
    WriteWord(memory, state.sp - 96u, state.sp);
    state.sp -= 96u;
    R(state, 31) = R(state, 3);
    state.lr = 0x827cad98u;
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-2094792704});
    ReadWord(memory, state, 3, R(state, 11) + 22280u);
    R(state, 4) = 0u;
    R(state, 5) = R(state, 31);
    state.lr = 0x827cada4u;
    dependencies.free_tail.Call(0x823ade28u, memory, state);
    CompareU(state, R(state, 3), 0u, state.cr0);
    R(state, 3) = 0u;
    if (state.cr0.eq) R(state, 3) = R(state, 31);
    state.sp += 96u;
    ReadWord(memory, state, 12, state.sp - 8u);
    state.lr = R(state, 12);
    R(state, 31) = ReadU64(memory, Address(state.sp - 16u));
}

void FreeGeneral(GuestMemory& memory, Dependencies dependencies,
    Integer& state)
{
    R(state, 11) = WordRotateMask(R(state, 4), 0, 0x80000000u);
    CompareS(state, R(state, 11), 0u, state.cr0);
    if (state.cr0.eq)
    {
        FreeHeapGeneral(memory, dependencies, state);
        return;
    }
    CompareU(state, R(state, 3), 0u, state.cr6);
    if (state.cr6.eq) return;
    R(state, 4) = R(state, 3);
    R(state, 3) = 0u;
    dependencies.free_tail.Call(0x830d9edcu, memory, state);
}

void FreeDispatch(GuestMemory& memory, Dependencies dependencies,
    Integer& state)
{
    R(state, 11) = static_cast<std::uint64_t>(
        std::int64_t{-1484652544});
    R(state, 11) |= 7u;
    CompareU(state, R(state, 4), R(state, 11), state.cr6);
    if (state.cr6.eq)
    {
        R(state, 11) = static_cast<std::uint64_t>(
            std::int64_t{-2094792704});
        R(state, 4) = R(state, 3);
        R(state, 11) += 29096u;
        R(state, 3) = R(state, 11);
        dependencies.free_tail.Call(0x827c9c60u, memory, state);
    }
    else FreeGeneral(memory, dependencies, state);
}

void ReturnRecord(GuestMemory& memory, Dependencies dependencies,
    Integer& state)
{
    R(state, 10) = R(state, 3) + 772u;
    CompareU(state, R(state, 5), 132u, state.cr6);
    if (state.cr6.gt)
    {
        R(state, 3) = R(state, 4) - 12u;
        R(state, 4) = 1636630528u;
        R(state, 11) = WordRotateMask(R(state, 3), 0, 0xfffffffeu);
        ReadWord(memory, state, 10, R(state, 11) + 4u);
        ReadWord(memory, state, 9, R(state, 11));
        R(state, 10) = WordRotateMask(R(state, 10), 0, 0xfffffffeu);
        WriteWord(memory, R(state, 10), R(state, 9));
        ReadWord(memory, state, 10, R(state, 11));
        ReadWord(memory, state, 11, R(state, 11) + 4u);
        R(state, 10) = WordRotateMask(R(state, 10), 0, 0xfffffffeu);
        WriteWord(memory, R(state, 10), R(state, 11));
        FreeDispatch(memory, dependencies, state);
        return;
    }
    R(state, 11) = WordRotateMask(R(state, 5), 30, 0x3fffffffu);
    R(state, 11) -= 1u;
    R(state, 11) = WordRotateMask(R(state, 11), 2, 0xfffffffcu);
    ReadWord(memory, state, 9, R(state, 11) + R(state, 10));
    WriteWord(memory, R(state, 4), R(state, 9));
    WriteWord(memory, R(state, 11) + R(state, 10), R(state, 4));
}

void SaveLookup(GuestMemory& memory, Integer& state)
{
    const auto incoming_sp = state.sp;
    R(state, 12) = state.lr;
    state.lr = 0x83058ae0u;
    for (unsigned index = 24u; index <= 31u; ++index)
        WriteU64(memory, Address(incoming_sp -
            (33u - index) * 8u), R(state, index));
    WriteWord(memory, incoming_sp - 8u, R(state, 12));
    WriteWord(memory, incoming_sp - 160u, incoming_sp);
    state.sp -= 160u;
}

void RestoreLookup(GuestMemory& memory, Integer& state)
{
    state.sp += 160u;
    for (unsigned index = 24u; index <= 31u; ++index)
        R(state, index) = ReadU64(memory, Address(state.sp -
            (33u - index) * 8u));
    ReadWord(memory, state, 12, state.sp - 8u);
    state.lr = R(state, 12);
}

void Lookup(GuestMemory& memory, Dependencies dependencies,
    Registers& state)
{
    auto& g = state.integer;
    SaveLookup(memory, g);
    R(g, 28) = R(g, 4);
    R(g, 24) = R(g, 6);
    R(g, 30) = R(g, 3);
    R(g, 26) = R(g, 5);
    R(g, 11) = static_cast<std::uint32_t>(R(g, 24)) & 0xffu;
    CompareU(g, R(g, 28), 0u, g.cr6);
    if (!g.cr6.eq)
    {
        R(g, 9) = R(g, 26);
        R(g, 10) = R(g, 28);
        do
        {
            ReadWord(memory, g, 8, R(g, 9));
            g.xer_ca = std::uint8_t(
                static_cast<std::uint32_t>(R(g, 10)) > 0u);
            R(g, 10) -= 1u;
            CompareS(g, R(g, 10), 0u, g.cr0);
            R(g, 9) += 4u;
            R(g, 11) += R(g, 8);
        } while (!g.cr0.eq);
    }
    R(g, 10) = 7u;
    R(g, 10) = static_cast<std::uint32_t>(R(g, 11)) /
        static_cast<std::uint32_t>(R(g, 10));
    R(g, 10) = R(g, 10) * 7u;
    R(g, 11) -= R(g, 10);
    R(g, 11) += 15u;
    R(g, 11) = WordRotateMask(R(g, 11), 2, 0xfffffffcu);
    R(g, 27) = R(g, 11) + R(g, 30);
    R(g, 29) = R(g, 27);
    ReadWord(memory, g, 31, R(g, 29));
    CompareU(g, R(g, 31), 0u, g.cr0);
    if (!g.cr0.eq)
    {
        R(g, 25) = static_cast<std::uint32_t>(R(g, 24)) & 0xffu;
        do
        {
            ReadWord(memory, g, 11, R(g, 31) + 8u);
            R(g, 10) = WordRotateMask(R(g, 11), 0, 0x3f80u);
            CompareU(g, R(g, 10), 14592u, g.cr6);
            if (g.cr6.eq)
            {
                ReadWord(memory, g, 11, R(g, 31) + 28u);
                WriteWord(memory, R(g, 29), R(g, 11));
                ReadWord(memory, g, 11, R(g, 30) + 40u);
                R(g, 11) = WordRotateMask(R(g, 11), 0, 0x1000u);
                CompareS(g, R(g, 11), 0u, g.cr0);
                if (!g.cr0.eq)
                {
                    ReadWord(memory, g, 11, R(g, 30) + 540u);
                    WriteWord(memory, R(g, 31) + 28u, R(g, 11));
                    WriteWord(memory, R(g, 30) + 540u, R(g, 31));
                }
                else
                {
                    ReadWord(memory, g, 11, R(g, 31) + 8u);
                    R(g, 3) = R(g, 30);
                    R(g, 6) = WordRotateMask(R(g, 11), 18, 0x7u);
                    R(g, 5) = WordRotateMask(R(g, 11), 13, 0x7u);
                    R(g, 4) = WordRotateMask(R(g, 11), 25, 0x7fu);
                    g.lr = 0x83058b98u;
                    (void)legacy_descriptor_record_routes::Apply(
                        0x82fac238u, memory, g);
                    R(g, 5) = R(g, 3);
                    R(g, 4) = R(g, 31);
                    R(g, 3) = R(g, 30);
                    R(g, 6) = 34u;
                    g.lr = 0x83058bacu;
                    ReturnRecord(memory, dependencies, g);
                }
            }
            else
            {
                ReadWord(memory, g, 10, R(g, 31) + 16u);
                R(g, 10) = WordRotateMask(R(g, 10), 18, 0xffu);
                CompareU(g, R(g, 25), R(g, 10), g.cr6);
                if (!g.cr6.eq)
                    R(g, 29) = R(g, 31) + 28u;
                else
                {
                    R(g, 11) = WordRotateMask(R(g, 11), 18, 0x7u);
                    CompareU(g, R(g, 11), R(g, 28), g.cr6);
                    if (g.cr6.lt)
                        R(g, 29) = R(g, 31) + 28u;
                    else
                    {
                        R(g, 8) = 1u;
                        CompareU(g, R(g, 28), 0u, g.cr6);
                        if (static_cast<std::uint32_t>(R(g, 28)) != 0u)
                        {
                            R(g, 10) = R(g, 26);
                            R(g, 9) = R(g, 31) + 40u;
                            R(g, 11) = R(g, 28);
                            do
                            {
                                ReadWord(memory, g, 7, R(g, 10));
                                g.xer_ca = std::uint8_t(
                                    static_cast<std::uint32_t>(R(g, 11)) > 0u);
                                R(g, 11) -= 1u;
                                CompareS(g, R(g, 11), 0u, g.cr0);
                                ReadWord(memory, g, 6, R(g, 9));
                                R(g, 10) += 4u;
                                R(g, 9) += 4u;
                                R(g, 7) = R(g, 6) - R(g, 7);
                                R(g, 7) = std::countl_zero(
                                    static_cast<std::uint32_t>(R(g, 7)));
                                R(g, 7) = WordRotateMask(R(g, 7), 27, 1u);
                                R(g, 8) = R(g, 7) & R(g, 8);
                            } while (!g.cr0.eq);
                        }
                        R(g, 11) = static_cast<std::uint32_t>(R(g, 8)) & 0xffu;
                        CompareS(g, R(g, 11), 0u, g.cr0);
                        if (!g.cr0.eq)
                        {
                            R(g, 3) = R(g, 31);
                            RestoreLookup(memory, g);
                            return;
                        }
                        R(g, 29) = R(g, 31) + 28u;
                    }
                }
            }
            ReadWord(memory, g, 31, R(g, 29));
            CompareU(g, R(g, 31), 0u, g.cr0);
        } while (!g.cr0.eq);
    }
    R(g, 6) = R(g, 24);
    R(g, 5) = R(g, 26);
    R(g, 4) = R(g, 28);
    R(g, 3) = R(g, 30);
    g.lr = 0x83058c38u;
    (void)legacy_descriptor_array_allocation::Apply(0x83058590u,
        memory, dependencies.allocation, state);
    ReadWord(memory, g, 11, R(g, 27));
    WriteWord(memory, R(g, 3) + 28u, R(g, 11));
    WriteWord(memory, R(g, 27), R(g, 3));
    ReadWord(memory, g, 11, R(g, 30) + 88u);
    R(g, 11) += 1u;
    WriteWord(memory, R(g, 30) + 88u, R(g, 11));
    RestoreLookup(memory, g);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (entry)
    {
    case 0x83058ad8u: Lookup(memory, dependencies, registers); return true;
    case 0x82fac980u:
        ReturnRecord(memory, dependencies, registers.integer); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_array_lookup
