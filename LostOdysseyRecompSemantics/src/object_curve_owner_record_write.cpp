#include "lo_semantics/object_curve_owner_record_write.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/object_curve_pair_apply.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <climits>

namespace lo::semantic::gpu::object_curve_owner_record_write
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
void Mul(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) * F(FP(s, b))); }
void Add(Registers& s, unsigned d, unsigned a, unsigned b)
{ FP(s, d) = Single(F(FP(s, a)) + F(FP(s, b))); }
void Madd(Registers& s, unsigned d, unsigned a, unsigned b, unsigned c)
{ FP(s, d) = Single(std::fma(F(FP(s, a)), F(FP(s, b)), F(FP(s, c)))); }

class LowerServices final : public object_curve_sample::NativeServices,
    public object_sample_accumulator::NativeServices,
    public object_curve_record_displacement::NativeServices
{
public:
    explicit LowerServices(object_curve_owner_record_write::NativeServices& native)
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
    object_curve_owner_record_write::NativeServices& native_;
};

void RestoreSmall(GuestMemory& m, Registers& s)
{
    R(s, 1) += 96u;
    R(s, 12) = m.ReadU32(W(R(s, 1) - 8u));
    s.lr = R(s, 12);
    R(s, 31) = ReadU64(m, W(R(s, 1) - 16u));
}
void Singleton(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    const auto old_sp = W(R(s, 1));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    WriteU64(m, old_sp - 16u, R(s, 31));
    R(s, 1) -= 96u;
    m.WriteU32(W(R(s, 1)), old_sp);
    R(s, 31) = 0xffffffff83320000ull;
    R(s, 3) = m.ReadU32(0x833189fcu);
    CmpU(s, R(s, 3), 0u);
    if (s.cr6.eq)
    {
        R(s, 11) = 0xffffffff82190000ull;
        R(s, 3) = R(s, 11) - 15844u;
        s.lr = 0x8242c9c4u;
        native.CallColdDirect(0x82629f98u, m, s);
        m.WriteU32(0x833189fcu, W(R(s, 3)));
        s.lr = 0x8242c9ccu;
        native.CallColdDirect(0x8262a050u, m, s);
        R(s, 3) = m.ReadU32(0x833189fcu);
    }
    RestoreSmall(m, s);
}
void Owner(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    const auto old_sp = W(R(s, 1));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    WriteU64(m, old_sp - 16u, R(s, 31));
    R(s, 1) -= 96u;
    m.WriteU32(W(R(s, 1)), old_sp);
    R(s, 31) = R(s, 3);
    CmpU(s, R(s, 31), 0u);
    if (!s.cr6.eq)
    {
        s.lr = 0x8260cf80u;
        Singleton(m, native, s);
        R(s, 11) = m.ReadU32(W(R(s, 31) + 52u));
        CmpU(s, R(s, 11), 0u);
        while (!s.cr6.eq)
        {
            CmpU(s, R(s, 11), R(s, 3));
            if (s.cr6.eq) break;
            R(s, 11) = m.ReadU32(W(R(s, 11) + 60u));
            CmpU(s, R(s, 11), 0u);
        }
        if (s.cr6.eq && W(R(s, 11)) == W(R(s, 3)))
        {
            R(s, 3) = R(s, 31);
            RestoreSmall(m, s);
            return;
        }
        R(s, 11) = W(R(s, 3)) == 0u ? 32u : 0u;
        R(s, 11) = recovery_abi::WordRotateMask(R(s, 11), 27, 1u);
        CmpS(s, R(s, 11), 0u);
        if (!s.cr6.eq)
        {
            R(s, 3) = R(s, 31);
            RestoreSmall(m, s);
            return;
        }
    }
    R(s, 3) = 0u;
    RestoreSmall(m, s);
}
void Save(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto old_sp = W(R(s, 1));
    R(s, 12) = s.lr;
    s.lr = 0x8262afb8u;
    for (unsigned i = 26u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    DisableFlush(native, s);
    WriteU64(m, old_sp - 72u, FP(s, 30));
    WriteU64(m, old_sp - 64u, FP(s, 31));
    R(s, 1) -= 240u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore(GuestMemory& m, Registers& s)
{
    R(s, 1) += 240u;
    const auto old_sp = W(R(s, 1));
    FP(s, 30) = ReadU64(m, old_sp - 72u);
    FP(s, 31) = ReadU64(m, old_sp - 64u);
    for (unsigned i = 26u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, old_sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(old_sp - 8u);
    s.lr = R(s, 12);
}
void Vector(GuestMemory& m, LowerServices& lower, Registers& s,
    unsigned offset, std::uint32_t lr)
{
    R(s, 3) = R(s, 1) + offset;
    s.lr = lr;
    (void)object_curve_pair_apply::Apply(0x822c7fb8u, m, lower, s);
}
void Scalar(GuestMemory& m, LowerServices& lower, Registers& s,
    std::uint32_t lr)
{
    s.lr = lr;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
}
void Samples(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s)
{
    const auto parameter = R(s, 31), object = R(s, 27);
    R(s, 11) = m.ReadU32(W(parameter + 124u));
    R(s, 9) = m.ReadU32(W(parameter + 60u));
    R(s, 30) = R(s, 3);
    R(s, 11) = recovery_abi::WordRotateMask(R(s, 11), 1, 0xfffffffeu);
    R(s, 8) = m.ReadU32(W(parameter + 120u));
    R(s, 10) = m.ReadU32(W(parameter + 56u));
    R(s, 7) = 0u;
    R(s, 4) = object + 68u;
    R(s, 6) = m.ReadU32(W(parameter + 8u));
    Load(m, native, s, 1, parameter + 140u);
    R(s, 26) = R(s, 28) + 12u;
    R(s, 11) = m.ReadU16(W(R(s, 11) + R(s, 9)));
    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
        std::int64_t(S(R(s, 8))));
    R(s, 29) = R(s, 11) + R(s, 10);
    R(s, 28) = R(s, 29) + R(s, 28);
    Vector(m, lower, s, 96u, 0x8262b03cu);
    R(s, 7) = 0u;
    R(s, 6) = m.ReadU32(W(parameter + 8u));
    R(s, 4) = R(s, 30) + 68u;
    Load(m, native, s, 1, parameter + 140u);
    Vector(m, lower, s, 112u, 0x8262b054u);
    R(s, 7) = 0u;
    R(s, 4) = object + 96u;
    R(s, 6) = m.ReadU32(W(parameter + 8u));
    Load(m, native, s, 1, parameter + 140u);
    Vector(m, lower, s, 144u, 0x8262b06cu);
    R(s, 7) = 0u;
    R(s, 4) = R(s, 30) + 96u;
    R(s, 6) = m.ReadU32(W(parameter + 8u));
    Load(m, native, s, 1, parameter + 140u);
    Vector(m, lower, s, 128u, 0x8262b084u);
    R(s, 3) = object + 124u;
    R(s, 5) = m.ReadU32(W(parameter + 8u));
    Load(m, native, s, 1, parameter + 140u);
    Scalar(m, lower, s, 0x8262b094u);
    R(s, 3) = R(s, 30) + 124u;
    DisableFlush(native, s);
    FP(s, 30) = FP(s, 1);
    R(s, 5) = m.ReadU32(W(parameter + 8u));
    Load(m, native, s, 1, parameter + 140u);
    Scalar(m, lower, s, 0x8262b0a8u);
}
void Blend(GuestMemory& m, NativeServices& native, Registers& s)
{
    const auto sp = W(R(s, 1));
    R(s, 11) = 0xffffffff82190000ull;
    Load(m, native, s, 13, sp + 96u);
    Load(m, native, s, 10, sp + 112u);
    Mul(s, 13, 13, 31);
    Load(m, native, s, 12, sp + 100u);
    Load(m, native, s, 9, sp + 116u);
    Mul(s, 12, 12, 31);
    Load(m, native, s, 11, sp + 104u);
    Load(m, native, s, 0, R(s, 11) - 27252u);
    Mul(s, 11, 11, 31);
    FP(s, 0) = Single(F(FP(s, 0)) - F(FP(s, 31)));
    Load(m, native, s, 8, sp + 120u);
    R(s, 11) = R(s, 26) + R(s, 29);
    Mul(s, 10, 10, 0);
    Mul(s, 9, 9, 0);
    Mul(s, 8, 0, 8);
    Add(s, 13, 13, 10); Store(m, s, 13, sp + 80u);
    Add(s, 13, 12, 9); Store(m, s, 13, sp + 84u);
    Add(s, 13, 11, 8); Store(m, s, 13, sp + 88u);
    R(s, 10) = m.ReadU32(sp + 80u);
    R(s, 9) = m.ReadU32(sp + 84u);
    R(s, 8) = m.ReadU32(sp + 88u);
    m.WriteU32(W(R(s, 28)), W(R(s, 10)));
    m.WriteU32(W(R(s, 28) + 4u), W(R(s, 9)));
    m.WriteU32(W(R(s, 28) + 8u), W(R(s, 8)));
    Load(m, native, s, 13, sp + 128u);
    Load(m, native, s, 12, sp + 132u);
    Mul(s, 13, 13, 0);
    Load(m, native, s, 11, sp + 144u);
    Mul(s, 12, 12, 0);
    Load(m, native, s, 10, sp + 148u);
    Mul(s, 11, 11, 31);
    Load(m, native, s, 9, sp + 136u);
    Mul(s, 10, 10, 31);
    Load(m, native, s, 8, sp + 152u);
    Mul(s, 9, 9, 0);
    Mul(s, 8, 8, 31);
    Add(s, 13, 11, 13); Store(m, s, 13, sp + 80u);
    R(s, 10) = m.ReadU32(sp + 80u);
    m.WriteU32(W(R(s, 26) + R(s, 29)), W(R(s, 10)));
    Add(s, 13, 10, 12); Store(m, s, 13, sp + 84u);
    R(s, 9) = m.ReadU32(sp + 84u);
    Add(s, 13, 8, 9); Store(m, s, 13, sp + 88u);
    Mul(s, 13, 30, 31);
    R(s, 10) = m.ReadU32(sp + 88u);
    m.WriteU32(W(R(s, 11) + 4u), W(R(s, 9)));
    R(s, 9) = R(s, 11) + 12u;
    m.WriteU32(W(R(s, 11) + 8u), W(R(s, 10)));
    Madd(s, 0, 0, 1, 13);
    const double value = F(FP(s, 0));
    const std::int32_t word = value > double(INT_MAX) ? INT_MAX :
        value < double(INT_MIN) ? INT_MIN : std::int32_t(value);
    FP(s, 0) = std::uint64_t(std::int64_t(word));
    m.WriteU32(W(R(s, 9)), W(FP(s, 0)));
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    if (entry == 0x8242c998u) { Singleton(memory, native, state); return true; }
    if (entry == 0x8260cf60u) { Owner(memory, native, state); return true; }
    if (entry != 0x8262afb0u) return false;
    LowerServices lower(native);
    Save(memory, native, state);
    R(state, 11) = 0xffffffff83310000ull;
    FP(state, 31) = FP(state, 2);
    R(state, 27) = R(state, 3);
    R(state, 31) = R(state, 4);
    R(state, 28) = R(state, 5);
    R(state, 30) = R(state, 7);
    R(state, 11) = memory.ReadU32(0x83315ea4u);
    CmpS(state, R(state, 11), 1u);
    if (!state.cr6.eq)
    {
        R(state, 3) = memory.ReadU32(W(R(state, 31) + 4u));
        state.lr = 0x8262aff0u;
        (void)object_sample_accumulator::Apply(0x82607318u,
            memory, lower, state);
    }
    R(state, 3) = R(state, 30);
    state.lr = 0x8262aff8u;
    Owner(memory, native, state);
    Samples(memory, native, lower, state);
    Blend(memory, native, state);
    Restore(memory, state);
    return true;
}
} // namespace lo::semantic::gpu::object_curve_owner_record_write
