#include "lo_semantics/legacy_descriptor_array_callers.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>

namespace lo::semantic::gpu::legacy_descriptor_array_callers
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WordRotateMask;
using recovery_abi::WriteU64;
using Integer = crt_stream_operations::Registers;
using Condition = crt_stream_operations::Condition;

std::uint64_t& R(Integer& g, unsigned index)
{ return index == 1u ? g.sp : g.r[index]; }
void Store(GuestMemory& m, std::uint64_t address, std::uint64_t value)
{ m.WriteU32(Address(address), Address(value)); }
void Load(GuestMemory& m, Integer& g, unsigned index, std::uint64_t address)
{ R(g, index) = m.ReadU32(Address(address)); }
void Compare(Integer& g, std::uint64_t a, std::uint64_t b, Condition& cr)
{
    const auto x = Address(a), y = Address(b);
    cr = {std::uint8_t(x < y), std::uint8_t(x > y),
        std::uint8_t(x == y), g.xer_so};
}
void CompareS(Integer& g, std::uint64_t a, Condition& cr)
{
    const auto x = std::bit_cast<std::int32_t>(Address(a));
    cr = {std::uint8_t(x < 0), std::uint8_t(x > 0),
        std::uint8_t(x == 0), g.xer_so};
}
std::uint64_t Left(std::uint64_t value, std::uint64_t shift)
{
    const auto n = static_cast<std::uint8_t>(shift);
    return n & 32u ? 0u : Address(Address(value) << (n & 63u));
}
std::uint64_t Right(std::uint64_t value, std::uint64_t shift)
{
    const auto n = static_cast<std::uint8_t>(shift);
    return n & 32u ? 0u : Address(value) >> (n & 63u);
}
void DisableFlush(Registers& s, legacy_descriptor_array_allocation::Services& services)
{
    if (s.cached_fp_control & 0x8040u)
    {
        s.cached_fp_control &= ~0x8040u;
        services.SetHostFpControl(s.cached_fp_control);
    }
}
void LoadFloat(GuestMemory& m, Registers& s,
    legacy_descriptor_array_allocation::Services& services,
    std::uint64_t address)
{
    DisableFlush(s, services);
    volatile float value = std::bit_cast<float>(m.ReadU32(Address(address)));
    s.f0_bits = std::bit_cast<std::uint64_t>(double(value));
}
void StoreFloat(GuestMemory& m, const Registers& s, std::uint64_t address)
{
    volatile float value = float(std::bit_cast<double>(s.f0_bits));
    Store(m, address, std::bit_cast<std::uint32_t>(float(value)));
}
void Save(GuestMemory& m, Integer& g, unsigned first,
    unsigned frame, std::uint32_t return_pc)
{
    const auto incoming = g.sp;
    R(g, 12) = g.lr;
    if (return_pc) g.lr = return_pc;
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(m, Address(incoming - (33u - i) * 8u), R(g, i));
    Store(m, incoming - 8u, R(g, 12));
    Store(m, incoming - frame, incoming);
    g.sp -= frame;
}
void Restore(GuestMemory& m, Integer& g, unsigned first, unsigned frame)
{
    g.sp += frame;
    for (unsigned i = first; i <= 31u; ++i)
        R(g, i) = ReadU64(m, Address(g.sp - (33u - i) * 8u));
    Load(m, g, 12u, g.sp - 8u);
    g.lr = R(g, 12);
}
template<class T> std::uint64_t ConvertInteger(double value)
{
    constexpr T Max = std::numeric_limits<T>::max();
    constexpr T Min = std::numeric_limits<T>::min();
    if (value > double(Max)) return std::uint64_t(std::int64_t(Max));
    const auto truncated = std::trunc(value);
    const auto upper = std::ldexp(1.0, sizeof(T) * 8u - 1u);
    if (!std::isfinite(truncated) || truncated < double(Min) || truncated >= upper)
    { std::feraiseexcept(FE_INVALID); return std::uint64_t(std::int64_t(Min)); }
    return std::uint64_t(std::int64_t(static_cast<T>(truncated)));
}
void DiagnosticShim(GuestMemory& m, Dependencies dependencies, Registers& s)
{
    auto& g = s.integer;
    R(g, 12) = g.lr;
    Store(m, g.sp - 8u, R(g, 12));
    for (unsigned i = 5u; i <= 10u; ++i)
        WriteU64(m, Address(g.sp + 32u + (i - 5u) * 8u), R(g, i));
    Store(m, g.sp - 96u, g.sp);
    g.sp -= 96u;
    R(g, 11) = g.sp + 80u;
    R(g, 10) = g.sp + 128u;
    Store(m, R(g, 11), R(g, 10));
    Load(m, g, 5u, g.sp + 80u);
    g.lr = 0x82f99f80u;
    dependencies.diagnostic.Call(0x82f99d98u, m, s);
}
void Pairs(GuestMemory& m, Dependencies dependencies, Registers& s)
{
    auto& g = s.integer;
    Save(m, g, 31u, 128u, 0u);
    R(g, 11) = g.sp + 80u;
    R(g, 9) = 0u;
    R(g, 10) = R(g, 5);
    Compare(g, R(g, 4), 0u, g.cr6);
    Store(m, R(g, 11), R(g, 9));
    if (!g.cr6.eq)
    {
        R(g, 11) = 0u;
        R(g, 8) = g.sp + 96u;
        R(g, 9) = R(g, 4);
        do
        {
            Load(m, g, 5u, R(g, 10) + 4u);
            R(g, 31) = Address(R(g, 11)) & 31u;
            R(g, 7) = WordRotateMask(R(g, 11), 29, 0x1ffffffcu);
            LoadFloat(m, s, dependencies.lookup.allocation, R(g, 10));
            R(g, 6) = g.sp + 80u;
            StoreFloat(m, s, R(g, 8));
            g.xer_ca = std::uint8_t(Address(R(g, 9)) > 0u);
            R(g, 9) -= 1u;
            CompareS(g, R(g, 9), g.cr0);
            R(g, 8) += 4u;
            R(g, 11) += 2u;
            R(g, 10) += 8u;
            R(g, 5) = Left(R(g, 5), R(g, 31));
            Load(m, g, 31u, R(g, 7) + R(g, 6));
            R(g, 5) |= R(g, 31);
            Store(m, R(g, 7) + R(g, 6), R(g, 5));
        } while (!g.cr0.eq);
    }
    R(g, 5) = g.sp + 96u;
    Load(m, g, 6u, g.sp + 80u);
    g.lr = 0x83058cdcu;
    (void)legacy_descriptor_array_lookup::Apply(0x83058ad8u, m,
        dependencies.lookup, s);
    Restore(m, g, 31u, 128u);
}
void TypedDoubles(GuestMemory& m, Dependencies dependencies, Registers& s)
{
    auto& g = s.integer;
    Save(m, g, 28u, 144u, 0x83058cf8u);
    R(g, 10) = R(g, 5);
    Store(m, g.sp + 188u, R(g, 6));
    R(g, 5) = 0u;
    Compare(g, R(g, 4), 0u, g.cr6);
    if (!g.cr6.eq)
    {
        R(g, 11) = 0u;
        R(g, 9) = g.sp + 80u;
        do
        {
            R(g, 7) = R(g, 11) + 1u;
            R(g, 31) = 2u;
            R(g, 7) = Address(R(g, 7)) & 31u;
            R(g, 8) = Address(R(g, 11)) & 31u;
            R(g, 30) = UINT64_MAX;
            R(g, 29) = WordRotateMask(R(g, 11), 29, 0x1ffffffcu);
            R(g, 28) = g.sp + 188u;
            Load(m, g, 29u, R(g, 29) + R(g, 28));
            R(g, 7) = Left(R(g, 31), R(g, 7)) - 1u;
            R(g, 31) = Left(R(g, 30), R(g, 8));
            R(g, 7) &= R(g, 29);
            R(g, 7) &= R(g, 31);
            R(g, 8) = Right(R(g, 7), R(g, 8));
            Compare(g, R(g, 8), 1u, g.cr6);
            if (!g.cr6.lt)
            {
                if (!g.cr6.eq)
                {
                    Compare(g, R(g, 8), 3u, g.cr6);
                    if (!g.cr6.lt)
                    {
                        R(g, 4) = 4800u;
                        g.lr = 0x83058dbcu;
                        DiagnosticShim(m, dependencies, s);
                        return;
                    }
                    DisableFlush(s, dependencies.lookup.allocation);
                    s.f0_bits = ReadU64(m, Address(R(g, 10)));
                    s.f0_bits = ConvertInteger<std::int64_t>(
                        std::bit_cast<double>(s.f0_bits));
                }
                else
                {
                    DisableFlush(s, dependencies.lookup.allocation);
                    s.f0_bits = ReadU64(m, Address(R(g, 10)));
                    s.f0_bits = ConvertInteger<std::int32_t>(
                        std::bit_cast<double>(s.f0_bits));
                }
                DisableFlush(s, dependencies.lookup.allocation);
                Store(m, R(g, 9), s.f0_bits);
            }
            else
            {
                DisableFlush(s, dependencies.lookup.allocation);
                s.f0_bits = ReadU64(m, Address(R(g, 10)));
                s.f0_bits = std::bit_cast<std::uint64_t>(
                    double(float(std::bit_cast<double>(s.f0_bits))));
                StoreFloat(m, s, R(g, 9));
            }
            R(g, 5) += 1u;
            R(g, 11) += 2u;
            R(g, 10) += 8u;
            R(g, 9) += 4u;
            Compare(g, R(g, 5), R(g, 4), g.cr6);
        } while (g.cr6.lt);
    }
    R(g, 5) = g.sp + 80u;
    g.lr = 0x83058dacu;
    (void)legacy_descriptor_array_lookup::Apply(0x83058ad8u, m,
        dependencies.lookup, s);
    Restore(m, g, 28u, 144u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, Registers& registers)
{
    switch (entry)
    {
    case 0x83058c60u: Pairs(memory, dependencies, registers); return true;
    case 0x83058cf0u: TypedDoubles(memory, dependencies, registers); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_array_callers
