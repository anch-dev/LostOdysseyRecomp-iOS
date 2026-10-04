#include "lo_semantics/object_curve_owner_node_record_update.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/registered_constructor_family.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_owner_node_record_update
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

std::uint64_t& R(Registers& s, unsigned index) { return s.r[index]; }
std::uint64_t& FP(Registers& s, unsigned index)
{
    std::uint64_t* fields[] = {&s.f0_bits, &s.f1_bits, &s.f2_bits,
        &s.f3_bits, &s.f4_bits, &s.f5_bits, &s.f6_bits, &s.f7_bits,
        &s.f8_bits, &s.f9_bits, &s.f10_bits, &s.f11_bits, &s.f12_bits,
        &s.f13_bits, &s.f14_bits, &s.f15_bits, &s.f16_bits, &s.f17_bits,
        &s.f18_bits, &s.f19_bits, &s.f20_bits, &s.f21_bits, &s.f22_bits,
        &s.f23_bits, &s.f24_bits, &s.f25_bits, &s.f26_bits, &s.f27_bits,
        &s.f28_bits, &s.f29_bits, &s.f30_bits, &s.f31_bits};
    return *fields[index];
}
std::uint32_t W(std::uint64_t value) { return Address(value); }
std::int32_t S(std::uint64_t value)
{ return std::bit_cast<std::int32_t>(W(value)); }
double F(std::uint64_t value) { return std::bit_cast<double>(value); }
std::uint64_t Bits(double value) { return std::bit_cast<std::uint64_t>(value); }
std::uint64_t Single(double value) { return Bits(double(float(value))); }
void CmpS(Registers& s, std::uint64_t a, std::uint64_t b)
{
    s.cr6 = {std::uint8_t(S(a) < S(b)), std::uint8_t(S(a) > S(b)),
        std::uint8_t(S(a) == S(b)), s.xer_so};
}
void CmpU(Registers& s, std::uint64_t a, std::uint64_t b)
{
    s.cr6 = {std::uint8_t(W(a) < W(b)), std::uint8_t(W(a) > W(b)),
        std::uint8_t(W(a) == W(b)), s.xer_so};
}
void CmpF(Registers& s, unsigned a, unsigned b)
{
    const double x = F(FP(s, a)), y = F(FP(s, b));
    const bool un = std::isnan(x) || std::isnan(y);
    s.cr6 = {std::uint8_t(!un && x < y), std::uint8_t(!un && x > y),
        std::uint8_t(!un && x == y), std::uint8_t(un)};
}
void DisableFlush(NativeServices& native, Registers& s)
{
    constexpr std::uint32_t Mask = 0x8040u;
    if (s.cached_fp_control & Mask)
    {
        s.cached_fp_control &= ~Mask;
        native.SetHostFpControl(s.cached_fp_control);
    }
}
void Load(GuestMemory& m, NativeServices& native, Registers& s,
    unsigned dest, std::uint64_t address)
{
    DisableFlush(native, s);
    FP(s, dest) = LoadedSingle::FromWord(m.ReadU32(W(address))).FprBits();
}
void Store(GuestMemory& m, Registers& s, unsigned source,
    std::uint64_t address)
{ m.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(FP(s, source))))); }
void Mov(Registers& s, unsigned d, unsigned a) { FP(s, d) = FP(s, a); }
void Mul(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) * F(FP(s, b))); }
void Add(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) + F(FP(s, b))); }
void Sub(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) - F(FP(s, b))); }
void Madd(Registers& s, unsigned d, unsigned a, unsigned b, unsigned c)
{ FP(s, d) = Single(std::fma(F(FP(s, a)), F(FP(s, b)), F(FP(s, c)))); }
void Divs(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) / F(FP(s, b))); }

class LowerServices final : public object_curve_sample::NativeServices,
    public object_sample_accumulator::NativeServices,
    public object_curve_record_displacement::NativeServices
{
public:
    explicit LowerServices(object_curve_owner_node_record_update::NativeServices& native)
        : native_(native) {}
    void SetHostFpControl(std::uint32_t control) override
    { native_.SetHostFpControl(control); }
    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_curve_sample::Registers& state) override
    { native_.CallVirtual(target, memory, static_cast<Registers&>(state)); }
    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_curve_record_displacement::Registers& state) override
    { native_.CallVirtual(target, memory, state); }
private:
    object_curve_owner_node_record_update::NativeServices& native_;
};

void Save(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x82628e58u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 15u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    R(s, 12) = R(s, 1) - 144u;
    s.lr = 0x82628e60u;
    DisableFlush(native, s);
    for (unsigned i = 22u; i <= 31u; ++i)
        WriteU64(m, old_sp - 152u - (31u - i) * 8u, FP(s, i));
    R(s, 1) -= 384u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore(GuestMemory& m, NativeServices& native, Registers& s,
    std::uint32_t return_pc)
{
    R(s, 1) += 384u;
    R(s, 12) = R(s, 1) - 144u;
    s.lr = return_pc;
    DisableFlush(native, s);
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 22u; i <= 31u; ++i)
        FP(s, i) = ReadU64(m, old_sp - 152u - (31u - i) * 8u);
    for (unsigned i = 15u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, old_sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(old_sp - 8u);
    s.lr = R(s, 12);
}

bool FindNode(GuestMemory& m, Registers& s)
{
    R(s, 11) = m.ReadU32(W(R(s, 23) + 68u));
    CmpS(s, R(s, 11), 0u);
    if (s.cr6.eq)
    {
        R(s, 11) = m.ReadU32(W(R(s, 23) + 72u));
        CmpS(s, R(s, 11), 0u);
        if (s.cr6.eq) return false;
    }
    R(s, 11) = m.ReadU32(W(R(s, 29) + 8u));
    R(s, 9) = R(s, 16);
    R(s, 8) = m.ReadU32(W(R(s, 11) + 632u));
    CmpS(s, R(s, 8), 0u);
    if (!s.cr6.gt) return false;
    R(s, 10) = m.ReadU32(W(R(s, 11) + 628u));
    do
    {
        R(s, 19) = m.ReadU32(W(R(s, 10)));
        CmpU(s, R(s, 19), 0u);
        if (!s.cr6.eq)
        {
            R(s, 11) = m.ReadU32(W(R(s, 19) + 4u));
            R(s, 7) = m.ReadU32(W(R(s, 23) + 68u));
            R(s, 6) = m.ReadU32(W(R(s, 11) + 60u));
            CmpS(s, R(s, 6), R(s, 7));
            if (s.cr6.eq)
            {
                R(s, 11) = m.ReadU32(W(R(s, 11) + 64u));
                R(s, 7) = m.ReadU32(W(R(s, 23) + 72u));
                CmpS(s, R(s, 11), R(s, 7));
                if (s.cr6.eq) return true;
            }
        }
        R(s, 9) += 1u;
        R(s, 10) += 4u;
        CmpS(s, R(s, 9), R(s, 8));
    } while (s.cr6.lt);
    return false;
}

void GetSingleton(GuestMemory& m, Registers& s,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services)
{
    R(s, 12) = s.lr;
    const auto old_sp = W(R(s, 1));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    WriteU64(m, old_sp - 16u, R(s, 31));
    R(s, 1) -= 128u;
    m.WriteU32(W(R(s, 1)), old_sp);
    R(s, 31) = 0xffffffff83320000ull;
    R(s, 3) = m.ReadU32(0x833189f4u);
    CmpU(s, R(s, 3), 0u);
    std::uint64_t result = 0;
    (void)registered_constructor_family::Apply(0x8242c458u, m,
        manager_services, registration_services, R(s, 3), old_sp, result);
    R(s, 3) = result;
    R(s, 1) += 128u;
    R(s, 12) = m.ReadU32(W(R(s, 1) - 8u));
    s.lr = R(s, 12);
    R(s, 31) = ReadU64(m, W(R(s, 1) - 16u));
}

void SelectOwner(GuestMemory& m, Registers& s,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services)
{
    R(s, 30) = m.ReadU32(W(R(s, 29) + 16u));
    R(s, 16) = 0;
    CmpU(s, R(s, 31), 0u);
    if (s.cr6.eq) { R(s, 17) = R(s, 16); return; }
    s.lr = 0x82628ea8u;
    GetSingleton(m, s, manager_services, registration_services);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 52u));
    CmpU(s, R(s, 11), 0u);
    while (!s.cr6.eq)
    {
        CmpU(s, R(s, 11), R(s, 3));
        if (s.cr6.eq) { R(s, 17) = R(s, 31); return; }
        R(s, 11) = m.ReadU32(W(R(s, 11) + 60u));
        CmpU(s, R(s, 11), 0u);
    }
    R(s, 11) = W(R(s, 3)) == 0u ? 32u : 0u;
    R(s, 11) = recovery_abi::WordRotateMask(R(s, 11), 27, 1u);
    CmpS(s, R(s, 11), 0u);
    R(s, 17) = s.cr6.eq ? R(s, 16) : R(s, 31);
}

void TransformNode(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    R(s, 11) = m.ReadU32(W(R(s, 29) + 8u));
    const auto matrix = R(s, 11);
    Load(m, native, s, 12, sp + 88u);
    Load(m, native, s, 0, sp + 84u);
    Load(m, native, s, 13, sp + 80u);
    Load(m, native, s, 10, matrix + 144u); Mul(s, 10, 10, 12);
    Load(m, native, s, 9, matrix + 148u);
    Load(m, native, s, 8, matrix + 152u);
    Mul(s, 9, 9, 12); Mul(s, 12, 8, 12);
    Load(m, native, s, 8, matrix + 128u);
    Load(m, native, s, 7, matrix + 132u);
    Load(m, native, s, 6, matrix + 136u);
    Load(m, native, s, 5, matrix + 112u);
    Load(m, native, s, 4, matrix + 116u);
    Load(m, native, s, 3, matrix + 120u);
    Load(m, native, s, 2, matrix + 160u);
    Load(m, native, s, 1, matrix + 164u);
    Madd(s, 10, 8, 0, 10);
    Load(m, native, s, 8, matrix + 168u);
    Madd(s, 9, 7, 0, 9);
    Madd(s, 0, 6, 0, 12);
    Madd(s, 12, 5, 13, 10);
    Madd(s, 10, 4, 13, 9);
    Madd(s, 9, 3, 13, 0);
    Madd(s, 0, 2, 26, 12);
    Store(m, s, 0, sp + 128u);
    R(s, 11) = m.ReadU32(sp + 128u);
    Madd(s, 13, 1, 26, 10);
    Store(m, s, 13, sp + 132u);
    Madd(s, 12, 8, 26, 9);
    Store(m, s, 12, sp + 136u);
    m.WriteU32(sp + 80u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 132u);
    m.WriteU32(sp + 84u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 136u);
    m.WriteU32(sp + 88u, W(R(s, 11)));
}

void TransformRecord(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    R(s, 11) = m.ReadU32(W(R(s, 29) + 8u));
    const auto matrix = R(s, 11);
    Load(m, native, s, 0, R(s, 31) + 20u);
    Load(m, native, s, 13, R(s, 31) + 24u);
    Load(m, native, s, 12, R(s, 31) + 16u);
    Load(m, native, s, 10, matrix + 128u); Mul(s, 10, 10, 0);
    Load(m, native, s, 9, matrix + 132u); Mul(s, 9, 9, 0);
    Load(m, native, s, 8, matrix + 136u);
    Load(m, native, s, 7, matrix + 144u); Mul(s, 0, 8, 0);
    Load(m, native, s, 6, matrix + 148u);
    Load(m, native, s, 5, matrix + 152u);
    Load(m, native, s, 4, matrix + 112u);
    Load(m, native, s, 3, matrix + 116u);
    Load(m, native, s, 2, matrix + 120u);
    Load(m, native, s, 1, matrix + 160u);
    Load(m, native, s, 31, matrix + 164u);
    Madd(s, 10, 7, 13, 10);
    Load(m, native, s, 22, matrix + 168u);
    Madd(s, 9, 6, 13, 9);
    Madd(s, 0, 5, 13, 0);
    Madd(s, 13, 12, 4, 10);
    Madd(s, 10, 3, 12, 9);
    Madd(s, 12, 2, 12, 0);
    Madd(s, 0, 1, 26, 13);
    Store(m, s, 0, sp + 144u);
    R(s, 11) = m.ReadU32(sp + 144u);
    Madd(s, 13, 31, 26, 10);
    Store(m, s, 13, sp + 148u);
    Madd(s, 12, 22, 26, 12);
    Store(m, s, 12, sp + 152u);
    m.WriteU32(sp + 96u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 148u);
    m.WriteU32(sp + 100u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 152u);
    m.WriteU32(sp + 104u, W(R(s, 11)));
}

void AddDirectForce(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 11) = m.ReadU32(W(R(s, 23) + 104u));
    R(s, 11) = W(R(s, 11)) & 0x10000000u;
    CmpU(s, R(s, 11), 0u);
    if (s.cr6.eq) return;
    Load(m, native, s, 13, R(s, 28) + 8u);
    Load(m, native, s, 0, R(s, 31) + 48u);
    Add(s, 0, 13, 0); Store(m, s, 0, R(s, 31) + 48u);
    Load(m, native, s, 0, R(s, 28) + 12u);
    Load(m, native, s, 13, R(s, 31) + 52u);
    Add(s, 0, 0, 13); Store(m, s, 0, R(s, 31) + 52u);
    Load(m, native, s, 0, R(s, 28) + 16u);
    Load(m, native, s, 13, R(s, 31) + 56u);
    Add(s, 0, 0, 13); Store(m, s, 0, R(s, 31) + 56u);
}

void ApplyOne(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s)
{
    const auto sp = W(R(s, 1));
    R(s, 11) = m.ReadU16(W(R(s, 24)));
    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
        std::int64_t(S(R(s, 20))));
    R(s, 31) = R(s, 11) + R(s, 21);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 92u));
    R(s, 11) = W(R(s, 11)) & 1u;
    CmpS(s, R(s, 11), 0u);
    if (!s.cr6.eq) return;
    R(s, 28) = R(s, 31) + R(s, 15);
    R(s, 4) = m.ReadU32(W(R(s, 28)));
    CmpS(s, R(s, 4), std::uint64_t(-1));
    if (s.cr6.eq) { AddDirectForce(m, native, s); return; }
    R(s, 11) = m.ReadU32(W(R(s, 19)));
    R(s, 3) = R(s, 19);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 116u));
    s.ctr = R(s, 11);
    s.lr = 0x82629060u;
    native.CallVirtual(W(s.ctr) & ~3u, m, s);
    R(s, 30) = R(s, 3);
    CmpU(s, R(s, 30), 0u);
    if (s.cr6.eq) return;
    R(s, 11) = m.ReadU32(W(R(s, 28) + 4u));
    CmpU(s, R(s, 11), 0u);
    if (!s.cr6.eq)
    {
        CmpU(s, R(s, 30), R(s, 11));
        if (!s.cr6.eq)
        {
            R(s, 11) = m.ReadU32(W(R(s, 23) + 104u));
            R(s, 11) = W(R(s, 11)) & 0x20000000u;
            CmpU(s, R(s, 11), 0u);
            if (s.cr6.eq)
            {
                m.WriteU32(W(R(s, 28)), W(R(s, 18)));
                m.WriteU32(W(R(s, 28) + 4u), W(R(s, 16)));
                return;
            }
        }
    }
    R(s, 3) = R(s, 23) + 76u;
    R(s, 5) = m.ReadU32(W(R(s, 29) + 8u));
    Load(m, native, s, 1, R(s, 30) + 12u);
    s.lr = 0x826290acu;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    R(s, 3) = R(s, 17) + 76u;
    Mov(s, 29, 1);
    R(s, 5) = m.ReadU32(W(R(s, 29) + 8u));
    Load(m, native, s, 1, R(s, 30) + 12u);
    s.lr = 0x826290c0u;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    R(s, 10) = m.ReadU32(W(R(s, 31) + 16u));
    R(s, 11) = R(s, 30) + 16u;
    Mov(s, 28, 1);
    CmpS(s, R(s, 26), R(s, 25));
    Sub(s, 27, 23, 25);
    Mul(s, 0, 29, 25);
    m.WriteU32(sp + 96u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(R(s, 31) + 20u));
    m.WriteU32(sp + 100u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(R(s, 31) + 24u));
    Madd(s, 11, 27, 28, 0);
    m.WriteU32(sp + 104u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(R(s, 11)));
    m.WriteU32(sp + 80u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(R(s, 11) + 4u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 8u));
    m.WriteU32(sp + 84u, W(R(s, 10)));
    m.WriteU32(sp + 88u, W(R(s, 11)));
    if (!s.cr6.eq)
    {
        CmpS(s, R(s, 25), 0u);
        if (!s.cr6.eq) TransformNode(m, native, s);
        CmpS(s, R(s, 26), 0u);
        if (!s.cr6.eq) TransformRecord(m, native, s);
    }
    Load(m, native, s, 13, sp + 104u);
    Load(m, native, s, 0, sp + 88u);
    Sub(s, 0, 0, 13);
    Load(m, native, s, 12, sp + 100u);
    Load(m, native, s, 13, sp + 84u);
    Sub(s, 13, 13, 12); Store(m, s, 13, sp + 116u);
    Load(m, native, s, 10, sp + 96u);
    Store(m, s, 0, sp + 120u);
    Mul(s, 12, 13, 13);
    Load(m, native, s, 13, sp + 80u);
    Sub(s, 13, 13, 10); Store(m, s, 13, sp + 112u);
    Madd(s, 0, 0, 0, 12);
    Madd(s, 0, 13, 13, 0);
    FP(s, 31) = Single(std::sqrt(F(FP(s, 0))));
    CmpF(s, 31, 11);
    if (s.cr6.gt) return;
    R(s, 11) = m.ReadU32(W(R(s, 23) + 104u));
    R(s, 3) = R(s, 23) + 108u;
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    CmpU(s, R(s, 11), 0u);
    if (!s.cr6.eq)
    {
        Sub(s, 0, 29, 31);
        R(s, 5) = 0;
        Divs(s, 1, 0, 29);
        s.lr = 0x826292b0u;
        (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
        Sub(s, 0, 28, 31);
        R(s, 5) = 0;
        Mov(s, 31, 1);
        Divs(s, 1, 0, 28);
    }
    else
    {
        R(s, 5) = m.ReadU32(W(R(s, 29) + 8u));
        Load(m, native, s, 1, R(s, 30) + 12u);
        s.lr = 0x826292d0u;
        (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
        Mov(s, 31, 1);
        R(s, 5) = m.ReadU32(W(R(s, 29) + 8u));
        Load(m, native, s, 1, R(s, 30) + 12u);
    }
    R(s, 3) = R(s, 17) + 108u;
    s.lr = 0x826292e4u;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    Mul(s, 0, 31, 25);
    R(s, 3) = R(s, 1) + 112u;
    Madd(s, 31, 1, 27, 0);
    Mov(s, 1, 24);
    s.lr = 0x826292f8u;
    (void)object_curve_record_displacement::Apply(0x8229f208u,
        m, lower, s);
    Load(m, native, s, 0, R(s, 22) + 8u);
    Mul(s, 0, 31, 0);
    Load(m, native, s, 13, sp + 112u);
    Load(m, native, s, 12, sp + 116u);
    R(s, 11) = sp + 120u;
    Load(m, native, s, 11, sp + 120u);
    Load(m, native, s, 8, R(s, 31) + 48u);
    Load(m, native, s, 7, R(s, 31) + 52u);
    Load(m, native, s, 6, R(s, 31) + 56u);
    Mul(s, 13, 13, 0); Mul(s, 12, 12, 0); Mul(s, 0, 11, 0);
    Mul(s, 11, 13, 30); Mul(s, 10, 12, 30); Mul(s, 9, 0, 30);
    Add(s, 11, 8, 11); Store(m, s, 11, R(s, 31) + 48u);
    Add(s, 11, 7, 10); Store(m, s, 11, R(s, 31) + 52u);
    Add(s, 11, 6, 9); Store(m, s, 11, R(s, 31) + 56u);
    R(s, 11) = m.ReadU32(W(R(s, 23) + 104u));
    R(s, 11) = W(R(s, 11)) & 0x40000000u;
    CmpU(s, R(s, 11), 0u);
    if (!s.cr6.eq)
    {
        Mul(s, 13, 13, 30);
        Load(m, native, s, 11, R(s, 31) + 32u);
        Mul(s, 12, 12, 30);
        Load(m, native, s, 10, R(s, 31) + 36u);
        Mul(s, 0, 0, 30);
        Load(m, native, s, 9, R(s, 31) + 40u);
        Add(s, 13, 13, 11); Store(m, s, 13, R(s, 31) + 32u);
        Add(s, 13, 12, 10); Store(m, s, 13, R(s, 31) + 36u);
        Add(s, 0, 0, 9); Store(m, s, 0, R(s, 31) + 40u);
    }
    R(s, 11) = R(s, 30) + 48u;
    R(s, 10) = R(s, 28) + 8u;
    R(s, 9) = m.ReadU32(W(R(s, 11)));
    m.WriteU32(W(R(s, 10)), W(R(s, 9)));
    R(s, 9) = m.ReadU32(W(R(s, 11) + 4u));
    m.WriteU32(W(R(s, 10) + 4u), W(R(s, 9)));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 8u));
    m.WriteU32(W(R(s, 10) + 8u), W(R(s, 11)));
}

} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state)
{
    if (entry != 0x82628e50u) return false;
    LowerServices lower(native);
    Save(memory, native, state);
    R(state, 28) = 0xffffffff83310000ull;
    DisableFlush(native, state);
    Mov(state, 30, 1);
    R(state, 23) = R(state, 3);
    Mov(state, 25, 2);
    R(state, 29) = R(state, 4);
    R(state, 15) = R(state, 5);
    R(state, 31) = R(state, 7);
    R(state, 11) = memory.ReadU32(0x83315ea4u);
    CmpS(state, R(state, 11), 1u);
    if (!state.cr6.eq)
    {
        R(state, 3) = memory.ReadU32(W(R(state, 29) + 4u));
        state.lr = 0x82628e94u;
        (void)object_sample_accumulator::Apply(0x82607318u,
            memory, lower, state);
    }
    SelectOwner(memory, state, manager_services, registration_services);
    if (!FindNode(memory, state))
    {
        Restore(memory, native, state, 0x82628f60u);
        return true;
    }
    R(state, 11) = memory.ReadU32(0x83315ea4u);
    CmpS(state, R(state, 11), 1u);
    if (!state.cr6.eq)
    {
        R(state, 4) = R(state, 19);
        R(state, 3) = memory.ReadU32(W(R(state, 29) + 4u));
        state.lr = 0x82628f7cu;
        (void)object_sample_accumulator::Apply(0x82607318u,
            memory, lower, state);
    }
    R(state, 10) = memory.ReadU32(W(R(state, 19) + 16u));
    R(state, 9) = memory.ReadU32(W(R(state, 30) + 72u));
    R(state, 11) = memory.ReadU32(W(R(state, 29) + 124u));
    R(state, 21) = memory.ReadU32(W(R(state, 29) + 56u));
    R(state, 27) = R(state, 11) - 1u;
    R(state, 20) = memory.ReadU32(W(R(state, 29) + 120u));
    R(state, 10) = memory.ReadU32(W(R(state, 10) + 72u));
    R(state, 9) = memory.ReadU32(W(R(state, 9) + 68u));
    CmpS(state, R(state, 27), 0u);
    R(state, 11) = memory.ReadU32(W(R(state, 29) + 60u));
    R(state, 26) = recovery_abi::WordRotateMask(R(state, 9), 1, 1u);
    R(state, 10) = memory.ReadU32(W(R(state, 10) + 68u));
    R(state, 25) = recovery_abi::WordRotateMask(R(state, 10), 1, 1u);
    if (!state.cr6.lt)
    {
        R(state, 10) = std::uint64_t((W(R(state, 27)) << 1u) & 0xfffffffeu);
        R(state, 18) = std::uint64_t(-1);
        R(state, 24) = R(state, 10) + R(state, 11);
        R(state, 11) = 0xffffffff82190000ull;
        R(state, 10) = 0xffffffff82000000ull;
        R(state, 22) = R(state, 11) - 27252u;
        R(state, 11) = 0xffffffff82000000ull;
        Load(memory, native, state, 24, R(state, 10) + 14596u);
        Load(memory, native, state, 23, R(state, 22));
        Load(memory, native, state, 26, R(state, 11) + 3664u);
        do
        {
            ApplyOne(memory, native, lower, state);
            R(state, 27) -= 1u;
            R(state, 24) -= 2u;
            CmpS(state, R(state, 27), 0u);
        } while (!state.cr6.lt);
    }
    Restore(memory, native, state, 0x826293c4u);
    return true;
}
} // namespace lo::semantic::gpu::object_curve_owner_node_record_update
