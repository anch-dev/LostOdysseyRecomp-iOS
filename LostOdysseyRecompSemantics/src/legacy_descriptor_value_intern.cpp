#include "lo_semantics/legacy_descriptor_value_intern.h"

#include "lo_semantics/recovery_abi.h"

#include <bit>

namespace lo::semantic::gpu::legacy_descriptor_value_intern
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using recovery_abi::WordRotateMask;
using Integer = crt_stream_operations::Registers;
using Condition = crt_stream_operations::Condition;
std::uint64_t& R(Registers& s, unsigned i)
{ return i == 1u ? s.integer.sp : s.integer.r[i]; }
void Compare(Registers& s, std::uint64_t a, std::uint64_t b,
    Condition& cr, bool signed_words = false)
{
    const auto x = Address(a), y = Address(b);
    const auto sx = std::bit_cast<std::int32_t>(x);
    const auto sy = std::bit_cast<std::int32_t>(y);
    cr = {std::uint8_t(signed_words ? sx < sy : x < y),
        std::uint8_t(signed_words ? sx > sy : x > y),
        std::uint8_t(x == y), s.integer.xer_so};
}
void Save(GuestMemory& m, Registers& s, unsigned first,
    unsigned size, std::uint32_t return_pc)
{
    R(s, 12) = s.integer.lr;
    if (return_pc) s.integer.lr = return_pc;
    const auto sp = Address(R(s, 1));
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(m, sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(sp - 8u, Address(R(s, 12)));
    R(s, 1) -= size;
    m.WriteU32(Address(R(s, 1)), sp);
}
void Restore(GuestMemory& m, Registers& s, unsigned first, unsigned size)
{
    R(s, 1) += size;
    const auto sp = Address(R(s, 1));
    for (unsigned i = first; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
    s.integer.lr = R(s, 12);
}
void Load(GuestMemory& m, Registers& s, unsigned i, std::uint64_t a)
{ R(s, i) = m.ReadU32(Address(a)); }
void Store(GuestMemory& m, std::uint64_t a, std::uint64_t v)
{ m.WriteU32(Address(a), Address(v)); }
void Insert(std::uint64_t& dst, std::uint64_t src, int shift,
    std::uint64_t mask)
{ dst = WordRotateMask(src, shift, mask) | (dst & ~mask); }
std::uint64_t Shift(std::uint64_t x, std::uint64_t count)
{
    const auto n = std::uint8_t(count);
    return n & 0x20u ? 0u : std::uint32_t(Address(x) << (n & 0x3fu));
}
void AllocateReuse(GuestMemory& m, Services& services, Registers& s)
{
    Save(m, s, 31u, 96u, 0u);
    R(s, 31) = R(s, 4); R(s, 5) = 26u; R(s, 4) = 20u;
    s.integer.lr = 0x82fbd870u;
    (void)legacy_descriptor_array_allocation::Apply(0x82fb36b0u,
        m, services, s);
    R(s, 11) = 262144u;
    Load(m, s, 10u, R(s, 3)); R(s, 11) |= 57u;
    Store(m, R(s, 3) + 12u, R(s, 31));
    Insert(R(s, 10), R(s, 11), 7, 0x1fe0u);
    Insert(R(s, 10), R(s, 11), 7, 0xe000000u);
    Store(m, R(s, 3), R(s, 10));
    Load(m, s, 11u, R(s, 31) + 4u);
    Store(m, R(s, 3) + 8u, R(s, 11));
    Store(m, R(s, 31) + 4u, R(s, 3));
    Load(m, s, 10u, R(s, 31) + 8u);
    Load(m, s, 9u, R(s, 3));
    Insert(R(s, 9), R(s, 10), 11, 0xe000000u);
    Store(m, R(s, 3), R(s, 9));
    for (;;)
    {
        Compare(s, R(s, 11), 0u, s.integer.cr0);
        if (s.integer.cr0.eq) break;
        Load(m, s, 10u, R(s, 11));
        R(s, 10) = WordRotateMask(R(s, 10), 0, 0xe000000u);
        Compare(s, R(s, 10), 0u, s.integer.cr0, true);
        if (!s.integer.cr0.eq)
        {
            Load(m, s, 11u, R(s, 11));
            R(s, 10) = WordRotateMask(R(s, 11), 2, 1u);
            Compare(s, R(s, 10), 0u, s.integer.cr0, true);
            if (!s.integer.cr0.eq)
            {
                Load(m, s, 10u, R(s, 3));
                Insert(R(s, 11), R(s, 10), 0, 0xfffffffffe001fffull);
                R(s, 11) |= 0x40000000u;
                Store(m, R(s, 3), R(s, 11));
            }
            break;
        }
        Load(m, s, 11u, R(s, 11) + 8u);
    }
    Load(m, s, 11u, R(s, 31) + 8u);
    R(s, 11) = Address(R(s, 11)) & 1u;
    Compare(s, R(s, 11), 0u, s.integer.cr0, true);
    if (!s.integer.cr0.eq)
    {
        Load(m, s, 11u, R(s, 3)); R(s, 10) = 1u;
        Insert(R(s, 11), R(s, 10), 0, 0x1fu);
        Store(m, R(s, 3), R(s, 11));
    }
    Restore(m, s, 31u, 96u);
}
void StoreFloat(GuestMemory& m, Services& services, Registers& s,
    unsigned offset, std::uint64_t bits)
{
    if (s.cached_fp_control & 0x8040u)
    {
        s.cached_fp_control &= ~0x8040u;
        services.SetHostFpControl(s.cached_fp_control);
    }
    Store(m, R(s, 1) + offset,
        std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(bits))));
}
void Intern(GuestMemory& m, Services& services, Registers& s)
{
    Save(m, s, 23u, 192u, 0x83058900u);
    StoreFloat(m, services, s, 252u, s.f3_bits); R(s, 9) = 7u;
    StoreFloat(m, services, s, 260u, s.f4_bits); R(s, 31) = R(s, 4);
    StoreFloat(m, services, s, 244u, s.f2_bits); R(s, 29) = R(s, 3);
    StoreFloat(m, services, s, 236u, s.f1_bits); R(s, 28) = R(s, 5);
    StoreFloat(m, services, s, 96u, s.f1_bits);
    StoreFloat(m, services, s, 100u, s.f2_bits);
    StoreFloat(m, services, s, 104u, s.f3_bits);
    StoreFloat(m, services, s, 108u, s.f4_bits);
    Load(m, s, 10u, R(s, 1) + 252u);
    Load(m, s, 11u, R(s, 1) + 260u); R(s, 11) += R(s, 10);
    Load(m, s, 10u, R(s, 1) + 244u); R(s, 11) += R(s, 10);
    Load(m, s, 10u, R(s, 1) + 236u); R(s, 11) += R(s, 10);
    R(s, 10) = (R(s, 10) & 0xffffffff00000000ull) |
        std::uint32_t(Address(R(s, 11)) / Address(R(s, 9)));
    R(s, 10) *= 7u; R(s, 11) -= R(s, 10); R(s, 11) += 15u;
    R(s, 30) = WordRotateMask(R(s, 11), 2, 0xfffffffcu);
    Load(m, s, 4u, R(s, 30) + R(s, 31));
    Compare(s, R(s, 4), 0u, s.integer.cr0);
    if (!s.integer.cr0.eq)
    {
        R(s, 26) = 1u;
        do
        {
            Load(m, s, 11u, R(s, 4) + 16u);
            R(s, 11) = WordRotateMask(R(s, 11), 0, 0x3fc000u);
            Compare(s, R(s, 11), 0u, s.integer.cr0, true);
            if (s.integer.cr0.eq)
            {
                Load(m, s, 11u, R(s, 4) + 8u);
                R(s, 3) = WordRotateMask(R(s, 11), 18, 7u);
                Compare(s, R(s, 3), R(s, 28), s.integer.cr6);
                if (!s.integer.cr6.lt)
                {
                    R(s, 7) = R(s, 27) = R(s, 8) = 0u;
                    Compare(s, R(s, 3), 0u, s.integer.cr6);
                    if (!s.integer.cr6.eq)
                    {
                        R(s, 6) = R(s, 4) + 40u;
                        do
                        {
                            Load(m, s, 5u, R(s, 6)); R(s, 10) = R(s, 11) = 0u;
                            R(s, 9) = R(s, 1) + 96u;
                            do
                            {
                                Load(m, s, 25u, R(s, 9));
                                Compare(s, R(s, 5), R(s, 25), s.integer.cr6, true);
                                if (s.integer.cr6.eq)
                                {
                                    R(s, 25) = 3u;
                                    R(s, 24) = Shift(R(s, 8), R(s, 11));
                                    R(s, 23) = Shift(R(s, 26), R(s, 10));
                                    R(s, 7) |= R(s, 23);
                                    R(s, 25) = Shift(R(s, 25), R(s, 11));
                                    R(s, 27) = (R(s, 27) & ~R(s, 25)) | R(s, 24);
                                }
                                R(s, 11) += 2u; R(s, 10) += 1u; R(s, 9) += 4u;
                                Compare(s, R(s, 11), 8u, s.integer.cr6, true);
                            } while (s.integer.cr6.lt);
                            R(s, 8) += 1u; R(s, 6) += 4u;
                            Compare(s, R(s, 8), R(s, 3), s.integer.cr6);
                        } while (s.integer.cr6.lt);
                    }
                    R(s, 11) = Shift(R(s, 26), R(s, 28));
                    R(s, 10) = R(s, 7) + 1u; R(s, 11) -= 1u;
                    R(s, 11) &= R(s, 10);
                    Compare(s, R(s, 11), 0u, s.integer.cr0, true);
                    if (s.integer.cr0.eq)
                    {
                        R(s, 3) = R(s, 31); s.integer.lr = 0x83058a90u;
                        AllocateReuse(m, services, s);
                        Load(m, s, 8u, R(s, 1) + 84u);
                        R(s, 9) = Address(R(s, 27)) & 0xffu;
                        R(s, 10) = WordRotateMask(R(s, 28), 20, 0x700000u);
                        R(s, 8) = WordRotateMask(R(s, 8), 0, 0x10000u);
                        R(s, 10) |= R(s, 9); R(s, 9) = R(s, 8) | 2u;
                        R(s, 11) = R(s, 3);
                        R(s, 10) = WordRotateMask(R(s, 10), 5, 0xffffffe0u);
                        Store(m, R(s, 1) + 84u, R(s, 9));
                        Load(m, s, 9u, R(s, 11));
                        Store(m, R(s, 1) + 80u, R(s, 11));
                        R(s, 9) = WordRotateMask(R(s, 9), 0, 0xffffffffffffe01full);
                        R(s, 8) = ReadU64(m, Address(R(s, 1) + 80u));
                        R(s, 9) = WordRotateMask(R(s, 9), 0, 0xfffffffff1ffffffull);
                        R(s, 10) |= R(s, 9);
                        WriteU64(m, Address(R(s, 29)), R(s, 8));
                        Store(m, R(s, 11), R(s, 10));
                        goto done;
                    }
                }
            }
            Load(m, s, 4u, R(s, 4) + 28u);
            Compare(s, R(s, 4), 0u, s.integer.cr0);
        } while (!s.integer.cr0.eq);
    }
    R(s, 11) = R(s, 1) + 80u; R(s, 10) = 0u;
    R(s, 5) = R(s, 1) + 96u; R(s, 4) = R(s, 28); R(s, 3) = R(s, 31);
    Store(m, R(s, 11), R(s, 10)); Load(m, s, 6u, R(s, 1) + 80u);
    s.integer.lr = 0x83058a44u;
    (void)legacy_descriptor_array_allocation::Apply(0x83058590u, m, services, s);
    Load(m, s, 9u, R(s, 1) + 84u); R(s, 11) = R(s, 3);
    Load(m, s, 10u, R(s, 30) + R(s, 31));
    R(s, 9) = WordRotateMask(R(s, 9), 0, 0x10000u) | 1u;
    Store(m, R(s, 1) + 80u, R(s, 11)); Store(m, R(s, 11) + 28u, R(s, 10));
    Store(m, R(s, 30) + R(s, 31), R(s, 11));
    Load(m, s, 11u, R(s, 31) + 88u); Store(m, R(s, 1) + 84u, R(s, 9));
    R(s, 10) = ReadU64(m, Address(R(s, 1) + 80u)); R(s, 11) += 1u;
    WriteU64(m, Address(R(s, 29)), R(s, 10)); Store(m, R(s, 31) + 88u, R(s, 11));
done:
    R(s, 3) = R(s, 29); Restore(m, s, 23u, 192u);
}
} // namespace
bool Apply(GuestAddress entry, GuestMemory& memory, Services& services,
    Registers& state)
{
    switch (entry)
    {
    case 0x830588f8u: Intern(memory, services, state); return true;
    case 0x82fbd850u: AllocateReuse(memory, services, state); return true;
    default: return false;
    }
}
} // namespace lo::semantic::gpu::legacy_descriptor_value_intern
