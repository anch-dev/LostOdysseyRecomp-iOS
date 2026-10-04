#include "lo_semantics/object_curve_owner_record_displacement.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/registered_constructor_family.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_owner_record_displacement
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
void Msub(Registers& s, unsigned d, unsigned a, unsigned b, unsigned c)
{ FP(s, d) = Single(std::fma(F(FP(s, a)), F(FP(s, b)), -F(FP(s, c)))); }
void Divs(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) / F(FP(s, b))); }

class LowerServices final : public object_curve_sample::NativeServices,
    public object_sample_accumulator::NativeServices,
    public object_curve_record_displacement::NativeServices
{
public:
    explicit LowerServices(object_curve_owner_record_displacement::NativeServices& native)
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
    object_curve_owner_record_displacement::NativeServices& native_;
};

void Save(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x82627ca0u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 23u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    R(s, 12) = R(s, 1) - 80u;
    s.lr = 0x82627ca8u;
    DisableFlush(native, s);
    for (unsigned i = 14u; i <= 31u; ++i)
        WriteU64(m, old_sp - 88u - (31u - i) * 8u, FP(s, i));
    R(s, 1) -= 384u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 1) += 384u;
    R(s, 12) = R(s, 1) - 80u;
    s.lr = 0x82628320u;
    DisableFlush(native, s);
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 14u; i <= 31u; ++i)
        FP(s, i) = ReadU64(m, old_sp - 88u - (31u - i) * 8u);
    for (unsigned i = 23u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, old_sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(old_sp - 8u);
    s.lr = R(s, 12);
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
    R(s, 3) = m.ReadU32(0x833189f0u);
    CmpU(s, R(s, 3), 0u);
    std::uint64_t result = 0;
    (void)registered_constructor_family::Apply(0x8242c398u, m,
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
    R(s, 28) = m.ReadU32(W(R(s, 30) + 16u));
    CmpU(s, R(s, 31), 0u);
    if (s.cr6.eq) { R(s, 23) = 0u; return; }
    s.lr = 0x82627ce8u;
    GetSingleton(m, s, manager_services, registration_services);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 52u));
    CmpU(s, R(s, 11), 0u);
    while (!s.cr6.eq)
    {
        CmpU(s, R(s, 11), R(s, 3));
        if (s.cr6.eq) { R(s, 23) = R(s, 31); return; }
        R(s, 11) = m.ReadU32(W(R(s, 11) + 60u));
        CmpU(s, R(s, 11), 0u);
    }
    R(s, 11) = W(R(s, 3)) == 0u ? 32u : 0u;
    R(s, 11) = recovery_abi::WordRotateMask(R(s, 11), 27, 1u);
    CmpS(s, R(s, 11), 0u);
    R(s, 23) = s.cr6.eq ? 0u : R(s, 31);
}

void Transform(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    const auto object = R(s, 29), params = R(s, 30);
    R(s, 10) = m.ReadU32(W(object + 72u));
    R(s, 11) = m.ReadU32(W(params + 16u));
    m.WriteU32(sp + 96u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 76u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 72u));
    m.WriteU32(sp + 100u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 80u));
    R(s, 11) = m.ReadU32(W(R(s, 11) + 68u));
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    m.WriteU32(sp + 104u, W(R(s, 10)));
    CmpU(s, R(s, 11), 0u);
    R(s, 10) = m.ReadU32(W(object + 84u));
    R(s, 11) = m.ReadU32(W(object + 68u));
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    m.WriteU32(sp + 112u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 88u));
    m.WriteU32(sp + 116u, W(R(s, 10)));
    R(s, 10) = m.ReadU32(W(object + 92u));
    m.WriteU32(sp + 120u, W(R(s, 10)));
    if (s.cr6.eq)
    {
        CmpU(s, R(s, 11), 0u);
        if (s.cr6.eq) return;
        R(s, 11) = m.ReadU32(W(params + 8u));
        const auto matrix = R(s, 11);
        Load(m, native, s, 0, object + 76u);
        Load(m, native, s, 13, object + 80u);
        Load(m, native, s, 12, object + 88u);
        Load(m, native, s, 10, object + 72u);
        Load(m, native, s, 11, object + 92u);
        Load(m, native, s, 8, matrix + 128u); Mul(s, 8, 8, 0);
        Load(m, native, s, 7, matrix + 132u);
        Load(m, native, s, 6, matrix + 136u);
        Mul(s, 7, 7, 0); Mul(s, 0, 6, 0);
        Load(m, native, s, 6, matrix + 144u);
        Load(m, native, s, 3, matrix + 148u);
        Mov(s, 1, 6);
        Load(m, native, s, 2, matrix + 152u);
        Mov(s, 31, 3);
        Load(m, native, s, 30, matrix + 112u);
        Load(m, native, s, 5, matrix + 128u);
        Load(m, native, s, 4, matrix + 132u);
        Mul(s, 5, 5, 12);
        Load(m, native, s, 26, matrix + 136u);
        Mul(s, 4, 4, 12); Mul(s, 12, 26, 12);
        Load(m, native, s, 29, matrix + 120u);
        Madd(s, 8, 6, 13, 8);
        Load(m, native, s, 6, matrix + 116u);
        Madd(s, 7, 3, 13, 7);
        Load(m, native, s, 9, object + 84u);
        Madd(s, 0, 2, 13, 0);
        Load(m, native, s, 13, matrix + 160u);
        Mov(s, 3, 30);
        Load(m, native, s, 2, matrix + 164u);
        Mov(s, 28, 6); Mov(s, 26, 29); Mov(s, 27, 13);
        Madd(s, 5, 1, 11, 5);
        Load(m, native, s, 1, matrix + 168u);
        Madd(s, 4, 31, 11, 4);
        Mov(s, 31, 2);
        Madd(s, 8, 30, 10, 8);
        Load(m, native, s, 30, matrix + 152u);
        Madd(s, 7, 6, 10, 7);
        Madd(s, 10, 29, 10, 0);
        Mov(s, 6, 1);
        Madd(s, 5, 9, 3, 5);
        Madd(s, 4, 28, 9, 4);
        Add(s, 0, 8, 13); Store(m, s, 0, sp + 128u);
        Madd(s, 8, 30, 11, 12);
        R(s, 11) = m.ReadU32(sp + 128u);
        Add(s, 13, 7, 2); Store(m, s, 13, sp + 132u);
        Add(s, 12, 10, 1); Store(m, s, 12, sp + 136u);
        m.WriteU32(sp + 96u, W(R(s, 11)));
        R(s, 11) = m.ReadU32(sp + 132u);
        Add(s, 11, 5, 27); Add(s, 10, 4, 31);
        Store(m, s, 11, sp + 128u); Store(m, s, 10, sp + 132u);
        Madd(s, 9, 26, 9, 8);
        m.WriteU32(sp + 100u, W(R(s, 11)));
        R(s, 11) = m.ReadU32(sp + 136u);
        Add(s, 9, 9, 6); Store(m, s, 9, sp + 136u);
    }
    else
    {
        CmpU(s, R(s, 11), 0u);
        if (!s.cr6.eq) return;
        R(s, 11) = m.ReadU32(W(params + 8u));
        const auto matrix = R(s, 11);
        Load(m, native, s, 13, object + 76u);
        Load(m, native, s, 0, object + 72u);
        Load(m, native, s, 12, object + 80u);
        Load(m, native, s, 10, object + 92u);
        Load(m, native, s, 9, object + 84u);
        Load(m, native, s, 7, matrix + 164u);
        Load(m, native, s, 8, matrix + 160u);
        Sub(s, 13, 13, 7); Sub(s, 0, 0, 8);
        Load(m, native, s, 8, matrix + 168u);
        Sub(s, 10, 10, 8);
        Load(m, native, s, 11, object + 88u);
        Sub(s, 12, 12, 8);
        Load(m, native, s, 8, matrix + 160u);
        Sub(s, 9, 9, 8);
        Load(m, native, s, 8, matrix + 116u); Mov(s, 5, 8);
        Load(m, native, s, 6, matrix + 144u);
        Sub(s, 11, 11, 7);
        Load(m, native, s, 7, matrix + 128u); Mov(s, 23, 7);
        Load(m, native, s, 2, matrix + 120u); Mov(s, 22, 6);
        Load(m, native, s, 4, matrix + 132u);
        Load(m, native, s, 1, matrix + 136u); Mov(s, 30, 2);
        Mul(s, 8, 8, 13);
        Load(m, native, s, 3, matrix + 148u);
        Mul(s, 7, 7, 0);
        Load(m, native, s, 31, matrix + 152u);
        Mul(s, 6, 6, 0);
        Load(m, native, s, 27, matrix + 112u);
        Mov(s, 26, 4); Mov(s, 29, 1); Mov(s, 28, 31);
        Mul(s, 5, 5, 11); Mul(s, 4, 4, 11); Mul(s, 11, 3, 11);
        Mov(s, 25, 3);
        Madd(s, 8, 2, 12, 8);
        Madd(s, 7, 1, 12, 7);
        Madd(s, 12, 31, 12, 6);
        Mov(s, 24, 27);
        Madd(s, 6, 30, 10, 5);
        Madd(s, 5, 29, 10, 4);
        Madd(s, 11, 28, 10, 11);
        Madd(s, 0, 27, 0, 8);
        Store(m, s, 0, sp + 128u);
        R(s, 11) = m.ReadU32(sp + 128u);
        Madd(s, 0, 26, 13, 7); Store(m, s, 0, sp + 132u);
        Madd(s, 0, 25, 13, 12); Store(m, s, 0, sp + 136u);
        m.WriteU32(sp + 96u, W(R(s, 11)));
        Madd(s, 0, 24, 9, 6);
        R(s, 11) = m.ReadU32(sp + 132u);
        Store(m, s, 0, sp + 128u);
        Madd(s, 0, 23, 9, 5); Store(m, s, 0, sp + 132u);
        Madd(s, 0, 22, 9, 11);
        m.WriteU32(sp + 100u, W(R(s, 11)));
        R(s, 11) = m.ReadU32(sp + 136u);
        Store(m, s, 0, sp + 136u);
    }
    m.WriteU32(sp + 104u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 128u);
    m.WriteU32(sp + 112u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 132u);
    m.WriteU32(sp + 116u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 136u);
    m.WriteU32(sp + 120u, W(R(s, 11)));
}

void PrepareDirection(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s)
{
    const auto sp = W(R(s, 1));
    Load(m, native, s, 0, sp + 112u);
    R(s, 3) = R(s, 1) + 128u;
    Load(m, native, s, 17, sp + 96u);
    Sub(s, 31, 0, 17);
    Store(m, s, 31, sp + 144u);
    R(s, 11) = m.ReadU32(sp + 144u);
    Load(m, native, s, 0, sp + 116u);
    Load(m, native, s, 16, sp + 100u);
    Sub(s, 30, 0, 16);
    Store(m, s, 30, sp + 148u);
    Load(m, native, s, 0, sp + 120u);
    Load(m, native, s, 15, sp + 104u);
    m.WriteU32(sp + 128u, W(R(s, 11)));
    Sub(s, 29, 0, 15);
    R(s, 11) = m.ReadU32(sp + 148u);
    Store(m, s, 29, sp + 152u);
    m.WriteU32(sp + 132u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 152u);
    m.WriteU32(sp + 136u, W(R(s, 11)));
    R(s, 11) = 0xffffffff82000000ull;
    Load(m, native, s, 1, R(s, 11) + 14596u);
    Store(m, s, 1, sp + 88u);
    s.lr = 0x82628004u;
    (void)object_curve_record_displacement::Apply(0x8229f208u,
        m, lower, s);
}

void SampleRadius(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s, std::uint64_t curve,
    std::uint32_t out_offset, std::uint32_t return_pc)
{
    const auto sp = W(R(s, 1));
    R(s, 3) = curve + 96u;
    R(s, 5) = m.ReadU32(W(R(s, 30) + 8u));
    DisableFlush(native, s);
    Mov(s, 1, 23);
    R(s, 11) = m.ReadU32(W(R(s, 3) + 24u));
    CmpU(s, R(s, 11), 0u);
    if (!s.cr6.eq)
    {
        R(s, 3) = W(R(s, 11));
        R(s, 11) = m.ReadU32(W(R(s, 3)));
        R(s, 11) = m.ReadU32(W(R(s, 11) + 268u));
        s.ctr = R(s, 11);
        s.lr = return_pc;
        native.CallVirtual(W(s.ctr) & ~3u, m, s);
    }
    else
    {
        R(s, 7) = 0;
        R(s, 6) = 1;
        R(s, 5) = R(s, 1) + out_offset;
        s.lr = return_pc == 0x826281b4u ? 0x826281ccu : 0x82628210u;
        (void)object_curve_sample::Apply(0x822c73e8u, m, lower, s);
        Load(m, native, s, 1, sp + out_offset);
    }
}

void ApplyOne(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s)
{
    const auto sp = W(R(s, 1));
    R(s, 11) = m.ReadU16(W(R(s, 28)));
    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
        std::int64_t(S(R(s, 24))));
    R(s, 31) = R(s, 11) + R(s, 25);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 92u));
    R(s, 11) = W(R(s, 11)) & 1u;
    CmpS(s, R(s, 11), 0u);
    if (!s.cr6.eq) return;
    R(s, 11) = R(s, 31) + 16u;
    Mul(s, 0, 30, 30);
    Mov(s, 9, 28); Mov(s, 8, 28); Mov(s, 7, 28);
    CmpF(s, 31, 28);
    R(s, 10) = m.ReadU32(W(R(s, 11) + 4u));
    m.WriteU32(sp + 148u, W(R(s, 10)));
    Load(m, native, s, 21, sp + 148u);
    Sub(s, 13, 21, 16);
    R(s, 10) = m.ReadU32(W(R(s, 11) + 8u));
    R(s, 11) = m.ReadU32(W(R(s, 11)));
    Madd(s, 0, 29, 29, 0);
    m.WriteU32(sp + 152u, W(R(s, 10)));
    Load(m, native, s, 19, sp + 152u);
    Sub(s, 12, 19, 15);
    m.WriteU32(sp + 144u, W(R(s, 11)));
    Load(m, native, s, 18, sp + 144u);
    Sub(s, 11, 18, 17);
    Mul(s, 13, 13, 30);
    Madd(s, 0, 31, 31, 0);
    Madd(s, 13, 12, 29, 13);
    Divs(s, 0, 20, 0);
    Madd(s, 13, 11, 31, 13);
    Mul(s, 12, 13, 30); Mul(s, 11, 13, 29);
    Mul(s, 10, 13, 31);
    Mul(s, 13, 12, 0); Mul(s, 12, 11, 0);
    Load(m, native, s, 11, sp + 136u);
    Mul(s, 0, 0, 10);
    Mul(s, 13, 13, 13);
    Madd(s, 13, 12, 12, 13);
    Load(m, native, s, 12, sp + 132u);
    Madd(s, 0, 0, 0, 13);
    Load(m, native, s, 13, sp + 128u);
    FP(s, 0) = Single(std::sqrt(F(FP(s, 0))));
    Mul(s, 13, 13, 0); Mul(s, 12, 12, 0);
    Mul(s, 0, 11, 0);
    Add(s, 25, 13, 17); Add(s, 26, 12, 16);
    Add(s, 27, 0, 15);
    if (!s.cr6.eq)
    {
        Sub(s, 0, 25, 17);
        Divs(s, 9, 0, 31);
    }
    CmpF(s, 30, 28);
    if (!s.cr6.eq)
    {
        Sub(s, 0, 26, 16);
        Divs(s, 8, 0, 30);
    }
    CmpF(s, 29, 28);
    if (!s.cr6.eq)
    {
        Sub(s, 0, 27, 15);
        Divs(s, 7, 0, 29);
    }
    Mov(s, 23, 28);
    CmpF(s, 9, 28);
    if (s.cr6.eq)
    {
        CmpF(s, 8, 28);
        if (s.cr6.eq)
        {
            CmpF(s, 7, 28);
            if (s.cr6.eq) goto candidate_done;
        }
        CmpF(s, 9, 28);
        if (s.cr6.eq)
        {
            CmpF(s, 8, 28);
            if (s.cr6.eq)
            {
                CmpF(s, 7, 28);
                if (s.cr6.eq) goto candidate_done;
                Mov(s, 23, 7);
            }
            else Mov(s, 23, 8);
        }
        else Mov(s, 23, 9);
    }
    else Mov(s, 23, 9);
    CmpF(s, 23, 28);
    if (s.cr6.lt) return;
    CmpF(s, 23, 20);
    if (s.cr6.gt) return;
candidate_done:
    SampleRadius(m, native, lower, s, R(s, 29), 80u, 0x826281b4u);
    DisableFlush(native, s);
    Mov(s, 24, 1);
    SampleRadius(m, native, lower, s, R(s, 23), 84u, 0x826281fcu);
    Sub(s, 26, 21, 26);
    Sub(s, 27, 19, 27);
    Sub(s, 22, 20, 14);
    Sub(s, 25, 18, 25);
    Mul(s, 0, 24, 14);
    Mul(s, 13, 26, 26);
    Madd(s, 0, 22, 1, 0);
    Madd(s, 13, 27, 27, 13);
    CmpF(s, 0, 28);
    Madd(s, 13, 25, 25, 13);
    FP(s, 13) = Single(std::sqrt(F(FP(s, 13))));
    if (!s.cr6.gt) return;
    CmpF(s, 13, 0);
    if (s.cr6.gt) return;
    Sub(s, 24, 0, 13);
    R(s, 3) = R(s, 29) + 124u;
    Divs(s, 23, 20, 0);
    R(s, 5) = m.ReadU32(W(R(s, 30) + 8u));
    Mul(s, 1, 23, 24);
    s.lr = 0x82628264u;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    Mul(s, 0, 27, 30);
    R(s, 3) = R(s, 1) + 112u;
    Mul(s, 13, 25, 29);
    Mul(s, 12, 26, 31);
    Mov(s, 21, 1);
    Load(m, native, s, 1, sp + 88u);
    Msub(s, 0, 26, 29, 0); Store(m, s, 0, sp + 112u);
    Msub(s, 0, 27, 31, 13); Store(m, s, 0, sp + 116u);
    Msub(s, 0, 25, 30, 12); Store(m, s, 0, sp + 120u);
    s.lr = 0x82628298u;
    (void)object_curve_record_displacement::Apply(0x8229f208u,
        m, lower, s);
    R(s, 3) = R(s, 23) + 124u;
    R(s, 5) = m.ReadU32(W(R(s, 30) + 8u));
    Mul(s, 1, 23, 24);
    s.lr = 0x826282a8u;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    Mul(s, 0, 21, 14);
    Load(m, native, s, 12, sp + 116u);
    Load(m, native, s, 11, sp + 120u);
    Load(m, native, s, 10, R(s, 31) + 48u);
    Load(m, native, s, 9, R(s, 31) + 52u);
    Load(m, native, s, 8, R(s, 31) + 56u);
    Madd(s, 13, 1, 22, 0);
    Load(m, native, s, 0, R(s, 26) + 8u);
    Mul(s, 0, 13, 0);
    Load(m, native, s, 13, sp + 112u);
    Mul(s, 13, 13, 0); Mul(s, 12, 12, 0); Mul(s, 0, 11, 0);
    Load(m, native, s, 11, sp + 428u);
    Mul(s, 13, 13, 11); Mul(s, 12, 12, 11); Mul(s, 0, 0, 11);
    Add(s, 13, 10, 13); Store(m, s, 13, R(s, 31) + 48u);
    Add(s, 13, 12, 9); Store(m, s, 13, R(s, 31) + 52u);
    Add(s, 0, 0, 8); Store(m, s, 0, R(s, 31) + 56u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state)
{
    if (entry != 0x82627c98u) return false;
    if (W(state.r[7]) != 0u && memory.ReadU32(0x833189f0u) == 0u)
        return false;
    LowerServices lower(native);
    Save(memory, native, state);
    R(state, 11) = 0xffffffff83320000ull;
    Store(memory, state, 1, R(state, 1) + 428u);
    R(state, 29) = R(state, 3);
    DisableFlush(native, state);
    Mov(state, 14, 2);
    R(state, 30) = R(state, 4);
    R(state, 31) = R(state, 7);
    R(state, 11) = memory.ReadU32(0x83315ea4u);
    CmpS(state, R(state, 11), 1u);
    if (!state.cr6.eq)
    {
        R(state, 3) = memory.ReadU32(W(R(state, 30) + 4u));
        state.lr = 0x82627cd8u;
        (void)object_sample_accumulator::Apply(0x82607318u,
            memory, lower, state);
    }
    SelectOwner(memory, state, manager_services, registration_services);
    Transform(memory, native, state);
    PrepareDirection(memory, native, lower, state);
    R(state, 11) = memory.ReadU32(W(R(state, 30) + 124u));
    R(state, 25) = memory.ReadU32(W(R(state, 30) + 56u));
    R(state, 27) = R(state, 11) - 1u;
    R(state, 24) = memory.ReadU32(W(R(state, 30) + 120u));
    R(state, 11) = memory.ReadU32(W(R(state, 30) + 60u));
    CmpS(state, R(state, 27), 0u);
    if (!state.cr6.lt)
    {
        R(state, 10) = std::uint64_t((W(R(state, 27)) << 1u) & 0xfffffffeu);
        R(state, 28) = R(state, 10) + R(state, 11);
        R(state, 11) = 0xffffffff82190000ull;
        R(state, 26) = R(state, 11) - 27252u;
        R(state, 11) = 0xffffffff82000000ull;
        Load(memory, native, state, 20, R(state, 26));
        Load(memory, native, state, 28, R(state, 11) + 3664u);
        do
        {
            ApplyOne(memory, native, lower, state);
            R(state, 27) -= 1u;
            R(state, 28) -= 2u;
            CmpS(state, R(state, 27), 0u);
        } while (!state.cr6.lt);
    }
    Restore(memory, native, state);
    return true;
}
} // namespace lo::semantic::gpu::object_curve_owner_record_displacement
