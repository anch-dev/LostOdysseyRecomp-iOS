#include "lo_semantics/legacy_descriptor_numeric_read.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cfenv>
#include <limits>

namespace lo::semantic::gpu::legacy_descriptor_numeric_read
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
using Condition = crt_stream_operations::Condition;
std::uint64_t& R(Registers& s, unsigned i)
{
    auto& g = s.numeric.classifier.integer;
    return i == 1u ? g.sp : g.r[i];
}
std::uint64_t Bits(double x) { return std::bit_cast<std::uint64_t>(x); }
double F(std::uint64_t x) { return std::bit_cast<double>(x); }
std::uint64_t Widen(std::uint32_t word)
{
    volatile float value = std::bit_cast<float>(word);
    return Bits(double(value));
}
void Cmp(Registers& s, std::uint64_t a, std::uint64_t b, Condition& cr,
    bool signed_words = false)
{
    const auto x = Address(a), y = Address(b);
    const auto sx = std::bit_cast<std::int32_t>(x);
    const auto sy = std::bit_cast<std::int32_t>(y);
    cr = {std::uint8_t(signed_words ? sx < sy : x < y),
        std::uint8_t(signed_words ? sx > sy : x > y),
        std::uint8_t(x == y), s.numeric.classifier.integer.xer_so};
}
void DisableFlush(Services& services, Registers& s)
{
    auto& csr = s.numeric.classifier.cached_fp_control;
    if (csr & 0x8040u)
    { csr &= ~0x8040u; services.SetHostFpControl(csr); }
}
void Load(GuestMemory& m, Registers& s, unsigned i, std::uint64_t a)
{ R(s, i) = m.ReadU32(Address(a)); }
void Store(GuestMemory& m, std::uint64_t a, std::uint64_t value)
{ m.WriteU32(Address(a), Address(value)); }
std::uint64_t Left(std::uint64_t x, std::uint64_t n)
{
    const auto shift = std::uint8_t(n);
    return shift & 32u ? 0u : std::uint32_t(Address(x) << (shift & 63u));
}
std::uint64_t Right(std::uint64_t x, std::uint64_t n)
{
    const auto shift = std::uint8_t(n);
    return shift & 32u ? 0u : Address(x) >> (shift & 63u);
}
void Save(GuestMemory& m, Registers& s, unsigned first, unsigned size,
    std::uint32_t return_pc)
{
    auto& g = s.numeric.classifier.integer;
    R(s, 12) = g.lr;
    if (return_pc) g.lr = return_pc;
    const auto sp = Address(R(s, 1));
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(m, sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(sp - 8u, Address(R(s, 12)));
    R(s, 1) -= size; Store(m, R(s, 1), sp);
}
void Restore(GuestMemory& m, Registers& s, unsigned first, unsigned size)
{
    R(s, 1) += size;
    for (unsigned i = first; i <= 31u; ++i)
        R(s, i) = ReadU64(m, Address(R(s, 1) - 16u - (31u - i) * 8u));
    Load(m, s, 12u, R(s, 1) - 8u);
    s.numeric.classifier.integer.lr = R(s, 12);
}
void GetScalar(GuestMemory& m, Services& services, Registers& s)
{
    auto& fp = s.numeric.classifier;
    auto& g = fp.integer;
    Save(m, s, 32u, 96u, 0u);
    R(s, 11) = R(s, 3);
    R(s, 10) = WordRotateMask(R(s, 11), 0, 0xfffff000u);
    Load(m, s, 10u, R(s, 10));
    Load(m, s, 3u, R(s, 10) + 148u);
    Load(m, s, 10u, R(s, 3) + 40u);
    R(s, 10) = ~R(s, 10);
    R(s, 10) = WordRotateMask(R(s, 10), 18, 1u);
    Cmp(s, R(s, 10), 0u, g.cr0, true);
    if (g.cr0.eq) R(s, 10) = 0u;
    else
    {
        Load(m, s, 10u, R(s, 11) + 16u);
        R(s, 9) = WordRotateMask(R(s, 4), 1, 0xfffffffeu);
        R(s, 10) = WordRotateMask(R(s, 10), 18, 0xffu);
        R(s, 10) = Right(R(s, 10), R(s, 9));
        R(s, 10) = Address(R(s, 10)) & 3u;
    }
    Cmp(s, R(s, 10), 1u, g.cr6);
    if (!g.cr6.lt)
    {
        bool signed_value = g.cr6.eq != 0;
        if (!signed_value)
        {
            Cmp(s, R(s, 10), 3u, g.cr6);
            if (!g.cr6.lt)
            { R(s, 4) = 4800u; g.lr = 0x82ff9bc4u; services.Diagnostic(m, s); }
        }
        R(s, 10) = R(s, 4) + 10u;
        R(s, 10) = WordRotateMask(R(s, 10), 2, 0xfffffffcu);
        Load(m, s, 11u, R(s, 10) + R(s, 11));
        if (signed_value)
            R(s, 11) = std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(R(s, 11)))));
        WriteU64(m, Address(R(s, 1) + 80u), R(s, 11));
        DisableFlush(services, s);
        fp.f0_bits = ReadU64(m, Address(R(s, 1) + 80u));
        DisableFlush(services, s);
        fp.f1_bits = Bits(double(std::bit_cast<std::int64_t>(fp.f0_bits)));
    }
    else
    {
        R(s, 10) = R(s, 4) + 10u;
        R(s, 10) = WordRotateMask(R(s, 10), 2, 0xfffffffcu);
        DisableFlush(services, s);
        fp.f1_bits = Widen(m.ReadU32(Address(R(s, 10) + R(s, 11))));
    }
    Restore(m, s, 32u, 96u);
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
void ReadRecord(GuestMemory& m, Services& services, Registers& s)
{
    auto& fp = s.numeric.classifier;
    auto& g = fp.integer;
    Save(m, s, 28u, 128u, 0x83058dc8u);
    R(s, 30) = R(s, 4); R(s, 29) = R(s, 6); R(s, 9) = R(s, 1) + 80u;
    R(s, 6) = 0u; R(s, 11) = WordRotateMask(R(s, 5), 1, 0xfffffffeu);
    Load(m, s, 10u, R(s, 30) + 16u); R(s, 31) = R(s, 3);
    R(s, 28) = R(s, 11) + 1u;
    R(s, 3) = WordRotateMask(R(s, 10), 18, 0xffu);
    Store(m, R(s, 9), R(s, 6)); R(s, 9) = Address(R(s, 28)) & 31u;
    R(s, 8) = 2u; R(s, 10) = Address(R(s, 11)) & 31u;
    Load(m, s, 6u, R(s, 1) + 80u);
    R(s, 11) = WordRotateMask(R(s, 11), 29, 0x1ffffffcu);
    R(s, 7) = UINT64_MAX; R(s, 6) |= R(s, 3); R(s, 4) = R(s, 1) + 80u;
    Cmp(s, R(s, 29), 0u, g.cr6); Store(m, R(s, 1) + 80u, R(s, 6));
    Load(m, s, 6u, R(s, 11) + R(s, 4)); R(s, 11) = Left(R(s, 8), R(s, 9));
    R(s, 11) -= 1u; R(s, 9) = Left(R(s, 7), R(s, 10));
    R(s, 11) &= R(s, 6); R(s, 11) &= R(s, 9);
    R(s, 11) = Right(R(s, 11), R(s, 10)); Store(m, R(s, 31) + 4u, R(s, 11));
    if (g.cr6.eq)
    {
        R(s, 11) = R(s, 5) + 10u;
        R(s, 11) = WordRotateMask(R(s, 11), 2, 0xfffffffcu);
        DisableFlush(services, s);
        fp.f0_bits = Widen(m.ReadU32(Address(R(s, 11) + R(s, 30))));
    }
    else
    {
        R(s, 4) = R(s, 5); R(s, 3) = R(s, 30); g.lr = 0x83058e5cu;
        GetScalar(m, services, s);
        R(s, 5) = R(s, 29); Load(m, s, 4u, R(s, 31) + 4u); g.lr = 0x83058e68u;
        (void)legacy_fp_flagged_routes::Apply(0x83053c78u, m, services, s);
        Load(m, s, 11u, R(s, 31) + 4u); Cmp(s, R(s, 11), 1u, g.cr6);
        if (!g.cr6.lt)
        {
            const bool word = g.cr6.eq != 0;
            if (!word)
            {
                Cmp(s, R(s, 11), 3u, g.cr6);
                if (!g.cr6.lt)
                {
                    R(s, 11) = WordRotateMask(R(s, 30), 0, 0xfffff000u);
                    R(s, 4) = 4800u; Load(m, s, 11u, R(s, 11));
                    Load(m, s, 3u, R(s, 11) + 148u); g.lr = 0x83058e94u;
                    services.Diagnostic(m, s);
                }
            }
            DisableFlush(services, s);
            fp.f0_bits = word ? ConvertInteger<std::int32_t>(F(fp.f1_bits)) :
                ConvertInteger<std::int64_t>(F(fp.f1_bits));
            R(s, 11) = R(s, 1) + 80u;
            DisableFlush(services, s); Store(m, R(s, 11), fp.f0_bits);
            fp.f0_bits = Widen(m.ReadU32(Address(R(s, 1) + 80u)));
        }
        else
        { DisableFlush(services, s); fp.f0_bits = Bits(double(float(F(fp.f1_bits)))); }
    }
    R(s, 3) = R(s, 31); DisableFlush(services, s);
    Store(m, R(s, 31), std::bit_cast<std::uint32_t>(float(F(fp.f0_bits))));
    Restore(m, s, 28u, 128u);
}
} // namespace
bool Apply(GuestAddress entry, GuestMemory& memory, Services& services, Registers& state)
{
    switch (entry)
    {
    case 0x83058dc0u: ReadRecord(memory, services, state); return true;
    case 0x82ff9b60u: GetScalar(memory, services, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_numeric_read
