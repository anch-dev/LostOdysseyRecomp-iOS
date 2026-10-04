#include "lo_semantics/object_curve_owner_vector_record.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/object_curve_pair_apply.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/registered_constructor_family.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_owner_vector_record
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
    explicit LowerServices(object_curve_owner_vector_record::NativeServices& native)
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
    object_curve_owner_vector_record::NativeServices& native_;
};

void Save(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x826297e8u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 22u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    R(s, 12) = R(s, 1) - 88u;
    s.lr = 0x826297f0u;
    DisableFlush(native, s);
    for (unsigned i = 19u; i <= 31u; ++i)
        WriteU64(m, old_sp - 96u - (31u - i) * 8u, FP(s, i));
    R(s, 1) -= 320u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 1) += 320u;
    R(s, 12) = R(s, 1) - 88u;
    s.lr = 0x82629c84u;
    DisableFlush(native, s);
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 19u; i <= 31u; ++i)
        FP(s, i) = ReadU64(m, old_sp - 96u - (31u - i) * 8u);
    for (unsigned i = 22u; i <= 31u; ++i)
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
    R(s, 3) = m.ReadU32(0x833189f8u);
    CmpU(s, R(s, 3), 0u);
    std::uint64_t result = 0;
    (void)registered_constructor_family::Apply(0x8242c518u, m,
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
    R(s, 22) = m.ReadU32(W(R(s, 30) + 16u));
    CmpU(s, R(s, 29), 0u);
    if (s.cr6.eq) return;
    s.lr = 0x82629834u;
    GetSingleton(m, s, manager_services, registration_services);
    R(s, 11) = m.ReadU32(W(R(s, 29) + 52u));
    CmpU(s, R(s, 11), 0u);
    while (!s.cr6.eq)
    {
        CmpU(s, R(s, 11), R(s, 3));
        if (s.cr6.eq) return;
        R(s, 11) = m.ReadU32(W(R(s, 11) + 60u));
        CmpU(s, R(s, 11), 0u);
    }
    R(s, 11) = W(R(s, 3)) == 0u ? 32u : 0u;
    R(s, 11) = recovery_abi::WordRotateMask(R(s, 11), 27, 1u);
    CmpS(s, R(s, 11), 0u);
    if (s.cr6.eq) R(s, 29) = 0u;
}
void SampleVector(GuestMemory& m, LowerServices& lower, Registers& s,
    unsigned offset, std::uint32_t return_pc)
{
    R(s, 3) = R(s, 1) + offset;
    s.lr = return_pc;
    (void)object_curve_pair_apply::Apply(0x822c7fb8u, m, lower, s);
}
void SampleCurve(GuestMemory& m, LowerServices& lower, Registers& s,
    std::uint32_t return_pc)
{
    s.lr = return_pc;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
}
void InitialSamples(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s)
{
    R(s, 7) = 0;
    Load(m, native, s, 1, R(s, 30) + 140u);
    R(s, 6) = R(s, 31);
    R(s, 4) = R(s, 26) + 72u;
    SampleVector(m, lower, s, 80u, 0x82629880u);
    R(s, 7) = 0;
    R(s, 6) = R(s, 31);
    Load(m, native, s, 1, R(s, 30) + 140u);
    R(s, 4) = R(s, 29) + 72u;
    SampleVector(m, lower, s, 96u, 0x82629898u);
    R(s, 5) = R(s, 31);
    R(s, 3) = R(s, 26) + 100u;
    Load(m, native, s, 1, R(s, 30) + 140u);
    SampleCurve(m, lower, s, 0x826298a8u);
    R(s, 5) = R(s, 31);
    R(s, 3) = R(s, 29) + 100u;
    Mov(s, 30, 1);
    Load(m, native, s, 1, R(s, 30) + 140u);
    SampleCurve(m, lower, s, 0x826298bcu);
}
void TransformVectors(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    R(s, 10) = m.ReadU32(W(R(s, 22) + 72u));
    R(s, 11) = 0xffffffff82190000ull;
    R(s, 23) = R(s, 11) - 27252u;
    R(s, 11) = m.ReadU32(W(R(s, 10) + 68u));
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    Load(m, native, s, 9, R(s, 23));
    Mov(s, 0, 9);
    CmpU(s, R(s, 11), 0u);
    Mov(s, 13, 9);
    Mov(s, 12, 9);
    if (!s.cr6.eq) return;
    const auto matrix = R(s, 31);
    Load(m, native, s, 8, matrix + 144u);
    Load(m, native, s, 10, sp + 88u); Mul(s, 8, 8, 10);
    Load(m, native, s, 7, matrix + 148u);
    Load(m, native, s, 6, matrix + 152u);
    Mul(s, 7, 7, 10); Mul(s, 10, 6, 10);
    Load(m, native, s, 6, matrix + 128u);
    Load(m, native, s, 0, sp + 84u);
    Load(m, native, s, 5, matrix + 132u);
    Load(m, native, s, 4, matrix + 136u);
    Load(m, native, s, 3, matrix + 112u);
    Load(m, native, s, 13, sp + 80u);
    Load(m, native, s, 2, matrix + 116u);
    Load(m, native, s, 31, matrix + 120u);
    Madd(s, 8, 6, 0, 8);
    Load(m, native, s, 29, matrix + 160u);
    Madd(s, 7, 5, 0, 7);
    Load(m, native, s, 26, matrix + 164u);
    Madd(s, 0, 4, 0, 10);
    Load(m, native, s, 6, matrix + 168u);
    Load(m, native, s, 12, sp + 100u);
    Load(m, native, s, 11, sp + 96u);
    Madd(s, 10, 3, 13, 8);
    Madd(s, 8, 2, 13, 7);
    Madd(s, 7, 31, 13, 0);
    Add(s, 0, 10, 29); Store(m, s, 0, sp + 112u);
    R(s, 11) = m.ReadU32(sp + 112u);
    Add(s, 13, 8, 26); Store(m, s, 13, sp + 116u);
    Add(s, 10, 7, 6); Store(m, s, 10, sp + 120u);
    Load(m, native, s, 0, sp + 104u);
    m.WriteU32(sp + 80u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 116u);
    m.WriteU32(sp + 84u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 120u);
    m.WriteU32(sp + 88u, W(R(s, 11)));
    Load(m, native, s, 13, matrix + 144u); Mul(s, 13, 13, 0);
    Load(m, native, s, 10, matrix + 148u);
    Load(m, native, s, 8, matrix + 152u);
    Mul(s, 10, 10, 0); Mul(s, 0, 8, 0);
    Load(m, native, s, 8, matrix + 128u);
    Load(m, native, s, 7, matrix + 132u);
    Load(m, native, s, 6, matrix + 136u);
    Load(m, native, s, 5, matrix + 112u);
    Load(m, native, s, 4, matrix + 116u);
    Load(m, native, s, 3, matrix + 120u);
    Load(m, native, s, 2, matrix + 160u);
    Load(m, native, s, 31, matrix + 164u);
    Madd(s, 13, 8, 12, 13);
    Load(m, native, s, 8, matrix + 168u);
    Madd(s, 10, 7, 12, 10);
    Madd(s, 0, 6, 12, 0);
    Madd(s, 13, 11, 5, 13);
    Madd(s, 12, 4, 11, 10);
    Madd(s, 11, 3, 11, 0);
    Add(s, 0, 13, 2); Store(m, s, 0, sp + 112u);
    R(s, 11) = m.ReadU32(sp + 112u);
    Add(s, 13, 12, 31); Store(m, s, 13, sp + 116u);
    Add(s, 12, 11, 8); Store(m, s, 12, sp + 120u);
    m.WriteU32(sp + 96u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 116u);
    m.WriteU32(sp + 100u, W(R(s, 11)));
    R(s, 11) = m.ReadU32(sp + 120u);
    m.WriteU32(sp + 104u, W(R(s, 11)));
    Load(m, native, s, 12, matrix + 584u);
    Load(m, native, s, 0, matrix + 588u);
    R(s, 11) = m.ReadU32(W(matrix + 76u));
    Load(m, native, s, 13, matrix + 592u);
    Mul(s, 0, 0, 12);
    Load(m, native, s, 11, matrix + 596u);
    Mul(s, 13, 13, 12);
    Mul(s, 12, 11, 12);
    CmpU(s, R(s, 11), 0u);
    if (s.cr6.eq) return;
    R(s, 10) = m.ReadU32(W(matrix + 600u));
    R(s, 10) = W(R(s, 10)) & 0x20000000u;
    CmpU(s, R(s, 10), 0u);
    if (!s.cr6.eq) return;
    Load(m, native, s, 11, R(s, 11) + 368u);
    Load(m, native, s, 10, R(s, 11) + 372u);
    Load(m, native, s, 8, R(s, 11) + 376u);
    Mul(s, 10, 10, 11);
    Load(m, native, s, 7, R(s, 11) + 380u);
    Mul(s, 8, 8, 11);
    Mul(s, 11, 7, 11);
    Mul(s, 0, 10, 0);
    Mul(s, 13, 8, 13);
    Mul(s, 12, 11, 12);
}
void PrepareBlend(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    Sub(s, 31, 9, 28);
    Load(m, native, s, 11, sp + 84u);
    Mul(s, 9, 13, 13);
    Load(m, native, s, 10, sp + 88u);
    Load(m, native, s, 13, sp + 80u);
    Mul(s, 11, 11, 28);
    Mul(s, 13, 13, 28);
    R(s, 11) = m.ReadU32(W(R(s, 30) + 124u));
    Mul(s, 10, 10, 28);
    R(s, 25) = m.ReadU32(W(R(s, 30) + 56u));
    R(s, 27) = R(s, 11) - 1u;
    R(s, 24) = m.ReadU32(W(R(s, 30) + 120u));
    R(s, 11) = m.ReadU32(W(R(s, 30) + 60u));
    CmpS(s, R(s, 27), 0u);
    Madd(s, 12, 12, 12, 9);
    Load(m, native, s, 9, sp + 104u);
    Mul(s, 9, 9, 31);
    Madd(s, 0, 0, 0, 12);
    Load(m, native, s, 12, sp + 100u);
    Mul(s, 12, 12, 31);
    Add(s, 19, 10, 9);
    FP(s, 26) = Single(std::sqrt(F(FP(s, 0))));
    Load(m, native, s, 0, sp + 96u);
    Mul(s, 25, 26, 30);
    Mul(s, 24, 26, 1);
    Mul(s, 0, 31, 0);
    Add(s, 20, 11, 12);
    Mul(s, 8, 25, 28);
    Add(s, 21, 13, 0);
    Madd(s, 22, 31, 24, 8);
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
    Load(m, native, s, 0, R(s, 31) + 16u);
    Sub(s, 0, 21, 0);
    Load(m, native, s, 13, R(s, 31) + 20u);
    Sub(s, 13, 20, 13);
    Load(m, native, s, 12, R(s, 31) + 24u);
    Store(m, s, 0, sp + 112u);
    Store(m, s, 13, sp + 116u);
    Mul(s, 11, 0, 0);
    Sub(s, 0, 19, 12);
    Store(m, s, 0, sp + 120u);
    Madd(s, 13, 13, 13, 11);
    Madd(s, 0, 0, 0, 13);
    FP(s, 30) = Single(std::sqrt(F(FP(s, 0))));
    CmpF(s, 30, 22);
    if (s.cr6.gt) return;
    R(s, 11) = m.ReadU32(W(R(s, 26) + 68u));
    R(s, 3) = R(s, 26) + 128u;
    R(s, 11) = W(R(s, 11)) & 0x40000000u;
    CmpU(s, R(s, 11), 0u);
    if (!s.cr6.eq)
    {
        Sub(s, 0, 25, 30);
        R(s, 5) = 0;
        Divs(s, 1, 0, 25);
        SampleCurve(m, lower, s, 0x82629b70u);
        Sub(s, 0, 24, 30);
        R(s, 5) = 0;
        Mov(s, 30, 1);
        Divs(s, 1, 0, 24);
    }
    else
    {
        R(s, 5) = m.ReadU32(W(R(s, 30) + 8u));
        Load(m, native, s, 1, R(s, 30) + 140u);
        SampleCurve(m, lower, s, 0x82629b90u);
        Mov(s, 30, 1);
        R(s, 5) = m.ReadU32(W(R(s, 30) + 8u));
        Load(m, native, s, 1, R(s, 30) + 140u);
    }
    R(s, 3) = R(s, 29) + 128u;
    SampleCurve(m, lower, s, 0x82629ba4u);
    R(s, 11) = m.ReadU32(W(R(s, 22) + 72u));
    Mov(s, 29, 1);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 68u));
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    CmpU(s, R(s, 11), 0u);
    if (s.cr6.eq)
    {
        Mul(s, 30, 30, 26);
        Mul(s, 29, 29, 26);
    }
    R(s, 3) = R(s, 1) + 112u;
    Mov(s, 1, 23);
    s.lr = 0x82629bd0u;
    (void)object_curve_record_displacement::Apply(0x8229f208u,
        m, lower, s);
    Mul(s, 0, 30, 28);
    Load(m, native, s, 12, sp + 116u);
    Load(m, native, s, 11, sp + 120u);
    Load(m, native, s, 8, R(s, 31) + 48u);
    Load(m, native, s, 7, R(s, 31) + 52u);
    Load(m, native, s, 6, R(s, 31) + 56u);
    Madd(s, 13, 31, 29, 0);
    Load(m, native, s, 0, R(s, 23) + 8u);
    Mul(s, 0, 13, 0);
    Load(m, native, s, 13, sp + 112u);
    Mul(s, 13, 13, 0);
    Mul(s, 12, 12, 0);
    Mul(s, 0, 11, 0);
    Mul(s, 11, 13, 27);
    Mul(s, 10, 12, 27);
    Mul(s, 9, 0, 27);
    Add(s, 11, 8, 11); Store(m, s, 11, R(s, 31) + 48u);
    Add(s, 11, 10, 7); Store(m, s, 11, R(s, 31) + 52u);
    Add(s, 11, 9, 6); Store(m, s, 11, R(s, 31) + 56u);
    R(s, 11) = m.ReadU32(W(R(s, 26) + 68u));
    R(s, 11) = W(R(s, 11)) & 0x20000000u;
    CmpU(s, R(s, 11), 0u);
    if (s.cr6.eq) return;
    Mul(s, 13, 13, 27);
    Load(m, native, s, 11, R(s, 31) + 32u);
    Mul(s, 12, 12, 27);
    Load(m, native, s, 10, R(s, 31) + 36u);
    Mul(s, 0, 0, 27);
    Load(m, native, s, 9, R(s, 31) + 40u);
    Add(s, 13, 11, 13); Store(m, s, 13, R(s, 31) + 32u);
    Add(s, 13, 10, 12); Store(m, s, 13, R(s, 31) + 36u);
    Add(s, 0, 9, 0); Store(m, s, 0, R(s, 31) + 40u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state)
{
    if (entry != 0x826297e0u) return false;
    LowerServices lower(native);
    Save(memory, native, state);
    R(state, 11) = 0xffffffff83310000ull;
    DisableFlush(native, state);
    Mov(state, 27, 1);
    R(state, 30) = R(state, 4);
    Mov(state, 28, 2);
    R(state, 26) = R(state, 3);
    R(state, 29) = R(state, 7);
    R(state, 11) = memory.ReadU32(0x83315ea4u);
    R(state, 31) = memory.ReadU32(W(R(state, 30) + 8u));
    CmpS(state, R(state, 11), 1u);
    if (!state.cr6.eq)
    {
        R(state, 3) = memory.ReadU32(W(R(state, 30) + 4u));
        state.lr = 0x82629824u;
        (void)object_sample_accumulator::Apply(0x82607318u,
            memory, lower, state);
    }
    SelectOwner(memory, state, manager_services, registration_services);
    InitialSamples(memory, native, lower, state);
    TransformVectors(memory, native, state);
    PrepareBlend(memory, native, state);
    if (!state.cr6.lt)
    {
        R(state, 10) = std::uint64_t((W(R(state, 27)) << 1u) & 0xfffffffeu);
        R(state, 28) = R(state, 10) + R(state, 11);
        R(state, 11) = 0xffffffff82000000ull;
        Load(memory, native, state, 23, R(state, 11) + 14596u);
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
} // namespace lo::semantic::gpu::object_curve_owner_vector_record
