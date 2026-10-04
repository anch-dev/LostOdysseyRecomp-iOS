#include "lo_semantics/object_refresh_routes.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace lo::semantic::gpu::object_refresh_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using Registers = object_child_float::Registers;

std::uint64_t& R(Registers& s, unsigned i) { return s.r[i]; }
std::uint32_t W(std::uint64_t v) { return Address(v); }

void CmpU(Registers& s, std::uint64_t a, std::uint64_t b)
{
    const auto x = W(a), y = W(b);
    s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y),
        std::uint8_t(x == y), s.xer_so};
}
void CmpS(Registers& s, std::uint64_t a, std::uint64_t b)
{
    const auto x = std::bit_cast<std::int32_t>(W(a));
    const auto y = std::bit_cast<std::int32_t>(W(b));
    s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y),
        std::uint8_t(x == y), s.xer_so};
}
void CmpD(Registers& s, std::uint64_t a, std::uint64_t b)
{
    s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b),
        std::uint8_t(a == b), s.xer_so};
}
void CmpF(Registers& s, std::uint64_t a, std::uint64_t b)
{
    const double x = std::bit_cast<double>(a), y = std::bit_cast<double>(b);
    const bool un = std::isnan(x) || std::isnan(y);
    s.cr6 = {std::uint8_t(!un && x < y), std::uint8_t(!un && x > y),
        std::uint8_t(!un && x == y), std::uint8_t(un)};
}
std::uint64_t RotMask(std::uint64_t value, unsigned shift,
    unsigned mb, unsigned me)
{
    // Match generated PPC's 64-bit repeated-word expression. Wrapped masks
    // retain its upper word, which is observable in the selected GPR ABI.
    const auto repeated = std::uint64_t(W(value)) | (value << 32u);
    std::uint64_t mask = mb > me ? 0xffffffff00000000ull : 0ull;
    for (unsigned bit = 0; bit < 32u; ++bit)
        if ((mb <= me && bit >= mb && bit <= me) ||
            (mb > me && (bit >= mb || bit <= me)))
            mask |= std::uint64_t{1} << (31u - bit);
    return std::rotl(repeated, int(shift)) & mask;
}
std::uint64_t InsertMask(std::uint64_t destination, std::uint64_t value,
    unsigned shift, unsigned mb, unsigned me)
{
    const auto inserted = RotMask(value, shift, mb, me);
    const auto mask = RotMask(0xffffffffu, 0, mb, me);
    return (destination & ~mask) | inserted;
}
void Save(GuestMemory& memory, Registers& s, unsigned first)
{
    const auto sp = W(R(s, 1));
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(memory, sp - 16u - (31u - i) * 8u, R(s, i));
    memory.WriteU32(sp - 8u, W(R(s, 12)));
}
void Restore(GuestMemory& memory, Registers& s, unsigned first)
{
    const auto sp = W(R(s, 1));
    for (unsigned i = first; i <= 31u; ++i)
        R(s, i) = ReadU64(memory, sp - 16u - (31u - i) * 8u);
    R(s, 12) = memory.ReadU32(sp - 8u);
    s.lr = R(s, 12);
}
void Push(GuestMemory& memory, Registers& s, std::uint32_t size)
{
    const auto old = W(R(s, 1));
    R(s, 1) -= size;
    memory.WriteU32(W(R(s, 1)), old);
}
void DisableFlush(Dependencies d, Registers& s)
{
    constexpr std::uint32_t Mask = 0x8040u;
    if (s.cached_fp_control & Mask)
    {
        s.cached_fp_control &= ~Mask;
        d.boundary.SetHostFpControl(s.cached_fp_control);
    }
}
std::uint64_t LoadF(GuestMemory& memory, Dependencies d,
    Registers& s, std::uint64_t address)
{
    DisableFlush(d, s);
    return LoadedSingle::FromWord(memory.ReadU32(W(address))).FprBits();
}
double F(std::uint64_t bits) { return std::bit_cast<double>(bits); }
std::uint64_t Bits(double value) { return std::bit_cast<std::uint64_t>(value); }
std::uint64_t Single(double value) { return Bits(double(float(value))); }
void StoreF(GuestMemory& memory, std::uint64_t address, std::uint64_t bits)
{
    memory.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(bits))));
}
void Direct(GuestAddress target, GuestAddress return_address,
    GuestMemory& memory, Dependencies d, Registers& s)
{
    s.lr = return_address;
    d.boundary.CallDirect(target, memory, s);
}
void Virtual(GuestAddress return_address, GuestMemory& memory,
    Dependencies d, Registers& s)
{
    s.lr = return_address;
    d.boundary.CallVirtual(W(s.ctr) & ~GuestAddress{3}, memory, s);
}

void RefreshChildren(GuestMemory& m, Dependencies d, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x826099b0u;
    Save(m, s, 25u);
    Push(m, s, 160u);
    R(s, 31) = R(s, 3);
    R(s, 11) = R(s, 31);
    CmpU(s, R(s, 31), 0);
    if (s.cr6.eq) goto loc_99e0;
loc_99c4:
    R(s, 10) = ReadU64(m, W(R(s, 11) + 8u));
    R(s, 10) = RotMask(R(s, 10), 0u, 21u, 22u);
    CmpD(s, R(s, 10), 0);
    if (!s.cr6.eq) goto loc_9cd0;
    R(s, 11) = m.ReadU32(W(R(s, 11) + 40u));
    CmpU(s, R(s, 11), 0);
    if (!s.cr6.eq) goto loc_99c4;
loc_99e0:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    CmpU(s, R(s, 11), 0);
    if (s.cr6.eq) goto loc_9cd0;
    s.f0_bits = LoadF(m, d, s, R(s, 11) + 72u);
    R(s, 3) = R(s, 11) + 76u;
    R(s, 11) = 0xffffffff82000000ull;
    StoreF(m, R(s, 31) + 708u, s.f0_bits);
    R(s, 5) = R(s, 31);
    s.f1_bits = LoadF(m, d, s, R(s, 11) + 3664u);
    Direct(0x822c7388u, 0x82609a08u, m, d, s);
    s.f0_bits = LoadF(m, d, s, R(s, 31) + 708u);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 632u));
    s.f0_bits = Single(F(s.f1_bits) + F(s.f0_bits));
    R(s, 25) = 0;
    StoreF(m, R(s, 31) + 708u, s.f0_bits);
    CmpS(s, R(s, 11), 0);
    if (!s.cr6.eq) goto loc_9ac8;
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 28) = R(s, 25);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 120u));
    CmpS(s, R(s, 11), 0);
    if (!s.cr6.gt) goto loc_9ab4;
    R(s, 29) = R(s, 31) + 628u;
    R(s, 30) = R(s, 25);
loc_9a40:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 116u));
    R(s, 3) = m.ReadU32(W(R(s, 11) + R(s, 30)));
    CmpU(s, R(s, 3), 0);
    if (s.cr6.eq) goto loc_9a7c;
    R(s, 11) = m.ReadU32(W(R(s, 3)));
    R(s, 4) = R(s, 31);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 264u));
    s.ctr = R(s, 11);
    Virtual(0x82609a68u, m, d, s);
    m.WriteU32(W(R(s, 1) + 80u), W(R(s, 3)));
    R(s, 4) = R(s, 1) + 80u;
    R(s, 3) = R(s, 29);
    Direct(0x825f41e8u, 0x82609a78u, m, d, s);
    goto loc_9a9c;
loc_9a7c:
    R(s, 6) = 8;
    R(s, 5) = 4;
    R(s, 4) = 1;
    R(s, 3) = R(s, 29);
    Direct(0x822c42d8u, 0x82609a90u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 29)));
    R(s, 10) = RotMask(R(s, 3), 2u, 0u, 29u);
    m.WriteU32(W(R(s, 11) + R(s, 10)), W(R(s, 25)));
loc_9a9c:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 28) += 1u;
    R(s, 30) += 4u;
    R(s, 11) = m.ReadU32(W(R(s, 11) + 120u));
    CmpS(s, R(s, 28), R(s, 11));
    if (s.cr6.lt) goto loc_9a40;
loc_9ab4:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 668u));
    R(s, 11) = RotMask(R(s, 11), 0u, 3u, 1u);
    m.WriteU32(W(R(s, 31) + 668u), W(R(s, 11)));
    goto loc_9cd0;
loc_9ac8:
    R(s, 10) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 10) = m.ReadU32(W(R(s, 10) + 120u));
    CmpS(s, R(s, 11), R(s, 10));
    if (!s.cr6.lt) goto loc_9b84;
    R(s, 28) = R(s, 31) + 628u;
loc_9adc:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 10) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 10) = RotMask(R(s, 10), 2u, 0u, 29u);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 116u));
    R(s, 30) = m.ReadU32(W(R(s, 11) + R(s, 10)));
    CmpU(s, R(s, 30), 0);
    if (s.cr6.eq) goto loc_9b50;
    R(s, 11) = m.ReadU32(W(R(s, 30)));
    R(s, 4) = R(s, 31);
    R(s, 3) = R(s, 30);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 264u));
    s.ctr = R(s, 11);
    Virtual(0x82609b10u, m, d, s);
    R(s, 29) = R(s, 3);
    R(s, 4) = R(s, 1) + 80u;
    R(s, 3) = R(s, 28);
    m.WriteU32(W(R(s, 1) + 80u), W(R(s, 29)));
    Direct(0x825f41e8u, 0x82609b24u, m, d, s);
    CmpU(s, R(s, 29), 0);
    if (s.cr6.eq) goto loc_9b70;
    R(s, 11) = m.ReadU32(W(R(s, 29)));
    R(s, 6) = 1;
    R(s, 5) = R(s, 31);
    R(s, 4) = R(s, 30);
    R(s, 3) = R(s, 29);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 16u));
    s.ctr = R(s, 11);
    Virtual(0x82609b4cu, m, d, s);
    goto loc_9b70;
loc_9b50:
    R(s, 6) = 8;
    R(s, 5) = 4;
    R(s, 4) = 1;
    R(s, 3) = R(s, 28);
    Direct(0x822c42d8u, 0x82609b64u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 28)));
    R(s, 10) = RotMask(R(s, 3), 2u, 0u, 29u);
    m.WriteU32(W(R(s, 11) + R(s, 10)), W(R(s, 25)));
loc_9b70:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 10) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 120u));
    CmpS(s, R(s, 10), R(s, 11));
    if (s.cr6.lt) goto loc_9adc;
loc_9b84:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 26) = R(s, 25);
    R(s, 27) = m.ReadU32(W(R(s, 31) + 712u));
    CmpS(s, R(s, 11), 0);
    if (!s.cr6.gt) goto loc_9c5c;
    R(s, 28) = R(s, 25);
loc_9b9c:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 628u));
    R(s, 30) = m.ReadU32(W(R(s, 28) + R(s, 11)));
    CmpU(s, R(s, 30), 0);
    if (s.cr6.eq) goto loc_9cd8;
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 10) = m.ReadU32(W(R(s, 11) + 120u));
    CmpS(s, R(s, 26), R(s, 10));
    if (!s.cr6.lt) goto loc_9c14;
    R(s, 11) = m.ReadU32(W(R(s, 11) + 116u));
    R(s, 29) = m.ReadU32(W(R(s, 28) + R(s, 11)));
    CmpU(s, R(s, 29), 0);
    if (s.cr6.eq) goto loc_9c14;
    R(s, 11) = m.ReadU32(W(R(s, 30)));
    R(s, 6) = 0;
    R(s, 5) = R(s, 31);
    R(s, 4) = R(s, 29);
    R(s, 3) = R(s, 30);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 16u));
    s.ctr = R(s, 11);
    Virtual(0x82609becu, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 30)));
    R(s, 3) = R(s, 30);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 20u));
    s.ctr = R(s, 11);
    Virtual(0x82609c00u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 29) + 220u));
    CmpS(s, R(s, 27), R(s, 11));
    if (s.cr6.lt) goto loc_9c48;
    R(s, 27) = R(s, 11) - 1u;
    goto loc_9c48;
loc_9c14:
    R(s, 11) = m.ReadU32(W(R(s, 30)));
    R(s, 3) = R(s, 30);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 112u));
    s.ctr = R(s, 11);
    Virtual(0x82609c28u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 30)));
    R(s, 4) = 1;
    R(s, 3) = R(s, 30);
    R(s, 11) = m.ReadU32(W(R(s, 11)));
    s.ctr = R(s, 11);
    Virtual(0x82609c40u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 628u));
loc_9c44:
    m.WriteU32(W(R(s, 28) + R(s, 11)), W(R(s, 25)));
loc_9c48:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 26) += 1u;
    R(s, 28) += 4u;
    CmpS(s, R(s, 26), R(s, 11));
    if (s.cr6.lt) goto loc_9b9c;
loc_9c5c:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 712u));
    CmpS(s, R(s, 27), R(s, 11));
    if (s.cr6.eq) goto loc_9c6c;
    m.WriteU32(W(R(s, 31) + 712u), W(R(s, 27)));
loc_9c6c:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 712u));
    CmpS(s, R(s, 11), 0);
    if (!s.cr6.lt) goto loc_9c7c;
    m.WriteU32(W(R(s, 31) + 712u), W(R(s, 25)));
loc_9c7c:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 8) = R(s, 25);
    CmpS(s, R(s, 11), 0);
    if (!s.cr6.gt) goto loc_9cd0;
    R(s, 9) = R(s, 25);
loc_9c90:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 628u));
    R(s, 11) = m.ReadU32(W(R(s, 9) + R(s, 11)));
    CmpU(s, R(s, 11), 0);
    if (s.cr6.eq) goto loc_9cbc;
    R(s, 10) = m.ReadU32(W(R(s, 31) + 712u));
    R(s, 7) = m.ReadU32(W(R(s, 11) + 4u));
    R(s, 6) = RotMask(R(s, 10), 2u, 0u, 29u);
    m.WriteU32(W(R(s, 11) + 12u), W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(R(s, 7) + 216u));
    R(s, 10) = m.ReadU32(W(R(s, 10) + R(s, 6)));
    m.WriteU32(W(R(s, 11) + 16u), W(R(s, 10)));
loc_9cbc:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 8) += 1u;
    R(s, 9) += 4u;
    CmpS(s, R(s, 8), R(s, 11));
    if (s.cr6.lt) goto loc_9c90;
loc_9cd0:
    R(s, 1) += 160u;
    Restore(m, s, 25u);
    return;
loc_9cd8:
    R(s, 10) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 10) = m.ReadU32(W(R(s, 10) + 116u));
    R(s, 29) = m.ReadU32(W(R(s, 10) + R(s, 28)));
    CmpU(s, R(s, 29), 0);
    if (s.cr6.eq) goto loc_9c44;
    R(s, 11) = m.ReadU32(W(R(s, 29)));
    R(s, 4) = R(s, 31);
    R(s, 3) = R(s, 29);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 264u));
    s.ctr = R(s, 11);
    Virtual(0x82609d04u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 628u));
    R(s, 30) = R(s, 3);
    R(s, 6) = 0;
    R(s, 5) = R(s, 31);
    R(s, 4) = R(s, 29);
    m.WriteU32(W(R(s, 11) + R(s, 28)), W(R(s, 30)));
    R(s, 11) = m.ReadU32(W(R(s, 30)));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 16u));
    s.ctr = R(s, 11);
    Virtual(0x82609d2cu, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 30)));
    R(s, 3) = R(s, 30);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 20u));
    s.ctr = R(s, 11);
    Virtual(0x82609d40u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 29) + 220u));
    CmpS(s, R(s, 27), R(s, 11));
    if (s.cr6.lt) goto loc_9c48;
    R(s, 27) = R(s, 11) - 1u;
    goto loc_9c48;
}

void UpdateOwner(GuestMemory& m, Dependencies d, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x8260a3f8u;
    Save(m, s, 28u);
    DisableFlush(d, s);
    WriteU64(m, W(R(s, 1) - 48u), s.f31_bits);
    Push(m, s, 144u);
    R(s, 11) = 0xffffffff83230000ull;
    R(s, 31) = R(s, 3);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 20632u));
    CmpS(s, R(s, 11), 0);
    if (s.cr6.eq) goto loc_a5b4;
    R(s, 11) = R(s, 31);
    CmpU(s, R(s, 31), 0);
    if (s.cr6.eq) goto loc_a43c;
loc_a420:
    R(s, 10) = ReadU64(m, W(R(s, 11) + 8u));
    R(s, 10) = RotMask(R(s, 10), 0u, 21u, 22u);
    CmpD(s, R(s, 10), 0);
    if (!s.cr6.eq) goto loc_a5b4;
    R(s, 11) = m.ReadU32(W(R(s, 11) + 40u));
    CmpU(s, R(s, 11), 0);
    if (!s.cr6.eq) goto loc_a420;
loc_a43c:
    R(s, 3) = R(s, 31);
    s.lr = 0x8260a444u;
    RefreshChildren(m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 80u));
    R(s, 11) = RotMask(R(s, 11), 0u, 0u, 0u);
    CmpU(s, R(s, 11), 0);
    if (s.cr6.eq) goto loc_a5b4;
    R(s, 11) = 0xffffffff82000000ull;
    s.f0_bits = LoadF(m, d, s, R(s, 31) + 708u);
    s.f31_bits = LoadF(m, d, s, R(s, 11) + 3664u);
    CmpF(s, s.f0_bits, s.f31_bits);
    if (s.cr6.eq) goto loc_a5b0;
    R(s, 10) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 29) = 0;
    R(s, 11) = m.ReadU32(W(R(s, 31) + 668u));
    CmpS(s, R(s, 10), 0);
    R(s, 10) = R(s, 11) | 524288u;
    R(s, 28) = RotMask(R(s, 11), 13u, 31u, 31u);
    m.WriteU32(W(R(s, 31) + 668u), W(R(s, 10)));
    if (!s.cr6.gt) goto loc_a4c4;
    R(s, 30) = 0;
loc_a48c:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 628u));
    R(s, 10) = m.ReadU32(W(R(s, 30) + R(s, 11)));
    CmpU(s, R(s, 10), 0);
    if (s.cr6.eq) goto loc_a4b0;
    R(s, 3) = W(R(s, 10));
    R(s, 11) = m.ReadU32(W(R(s, 3)));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 60u));
    s.ctr = R(s, 11);
    Virtual(0x8260a4b0u, m, d, s);
loc_a4b0:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 632u));
    R(s, 29) += 1u;
    R(s, 30) += 4u;
    CmpS(s, R(s, 29), R(s, 11));
    if (s.cr6.lt) goto loc_a48c;
loc_a4c4:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 624u));
    CmpU(s, R(s, 11), 0);
    if (s.cr6.eq) goto loc_a538;
    R(s, 11) = m.ReadU32(W(R(s, 31)));
    R(s, 3) = R(s, 31);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 584u));
    s.ctr = R(s, 11);
    Virtual(0x8260a4e4u, m, d, s);
    R(s, 11) = m.ReadU32(W(R(s, 3) + 60u));
    R(s, 11) = RotMask(R(s, 11), 0u, 0u, 0u);
    CmpU(s, R(s, 11), 0);
    if (s.cr6.eq) goto loc_a538;
    R(s, 5) = 1;
    R(s, 3) = m.ReadU32(W(R(s, 31) + 624u));
    R(s, 4) = 1;
    s.lr = 0x8260a504u;
    if (!object_child_float_record_chain::Apply(0x822c5e58u,
            m, d.child_chain, s))
        throw std::runtime_error("missing actual 822C5E58 lower");
    s.f0_bits = LoadF(m, d, s, R(s, 31) + 708u);
    s.f13_bits = Single(F(s.f0_bits) / F(s.f1_bits));
    R(s, 11) = R(s, 1) + 80u;
    // PPC fctiwz stores the converted low word. The selected oracle covers
    // finite values within the signed 32-bit range; exceptional FP remains
    // an explicit validation limit.
    m.WriteU32(W(R(s, 11)), W(std::int64_t(std::trunc(F(s.f13_bits)))));
    R(s, 11) = m.ReadU32(W(R(s, 1) + 80u));
    R(s, 11) = std::uint64_t(std::int64_t(
        std::bit_cast<std::int32_t>(W(R(s, 11)))));
    WriteU64(m, W(R(s, 1) + 80u), R(s, 11));
    s.f13_bits = ReadU64(m, W(R(s, 1) + 80u));
    s.f13_bits = Single(double(std::bit_cast<std::int64_t>(s.f13_bits)));
    s.f0_bits = Single(-(F(s.f13_bits) * F(s.f1_bits) - F(s.f0_bits)));
    StoreF(m, R(s, 31) + 708u, s.f0_bits);
loc_a538:
    R(s, 10) = m.ReadU32(W(R(s, 31) + 732u));
    R(s, 11) = m.ReadU32(W(R(s, 31) + 668u));
    R(s, 10) = RotMask(R(s, 10), 0u, 2u, 2u);
    R(s, 29) = RotMask(R(s, 11), 15u, 31u, 31u);
    CmpU(s, R(s, 10), 0);
    R(s, 10) = R(s, 11) | 131072u;
    m.WriteU32(W(R(s, 31) + 668u), W(R(s, 10)));
    if (!s.cr6.eq) goto loc_a59c;
    s.f0_bits = LoadF(m, d, s, R(s, 31) + 708u);
    CmpF(s, s.f0_bits, s.f31_bits);
    if (!s.cr6.gt) goto loc_a598;
    R(s, 30) = 0xffffffff82190000ull;
    s.f1_bits = LoadF(m, d, s, R(s, 30) - 27648u);
loc_a56c:
    R(s, 11) = m.ReadU32(W(R(s, 31)));
    R(s, 3) = R(s, 31);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 288u));
    s.ctr = R(s, 11);
    Virtual(0x8260a580u, m, d, s);
    s.f0_bits = LoadF(m, d, s, R(s, 31) + 708u);
    s.f1_bits = LoadF(m, d, s, R(s, 30) - 27648u);
    s.f0_bits = Single(F(s.f0_bits) - F(s.f1_bits));
    StoreF(m, R(s, 31) + 708u, s.f0_bits);
    CmpF(s, s.f0_bits, s.f31_bits);
    if (s.cr6.gt) goto loc_a56c;
loc_a598:
    StoreF(m, R(s, 31) + 708u, s.f31_bits);
loc_a59c:
    R(s, 11) = m.ReadU32(W(R(s, 31) + 668u));
    R(s, 29) = InsertMask(R(s, 29), R(s, 28), 2u, 29u, 29u);
    R(s, 11) = InsertMask(R(s, 11), R(s, 29), 17u, 14u, 14u);
    R(s, 11) = InsertMask(R(s, 11), R(s, 29), 17u, 12u, 12u);
    m.WriteU32(W(R(s, 31) + 668u), W(R(s, 11)));
loc_a5b0:
    StoreF(m, R(s, 31) + 724u, s.f31_bits);
loc_a5b4:
    R(s, 1) += 144u;
    DisableFlush(d, s);
    s.f31_bits = ReadU64(m, W(R(s, 1) - 48u));
    Restore(m, s, 28u);
}

} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    Dependencies dependencies, object_child_float::Registers& state)
{
    if (entry == 0x826099a8u)
    {
        RefreshChildren(memory, dependencies, state);
        return true;
    }
    if (entry == 0x8260a3f0u)
    {
        UpdateOwner(memory, dependencies, state);
        return true;
    }
    return false;
}

} // namespace lo::semantic::gpu::object_refresh_routes
