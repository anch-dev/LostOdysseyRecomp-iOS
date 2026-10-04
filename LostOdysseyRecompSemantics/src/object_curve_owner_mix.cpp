#include "lo_semantics/object_curve_owner_mix.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_curve_pair_apply.h"
#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_owner_mix
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
std::uint64_t& R(Registers& s, unsigned i) { return s.r[i]; }
std::uint32_t W(std::uint64_t v) { return Address(v); }
std::int32_t S(std::uint64_t v)
{ return std::bit_cast<std::int32_t>(W(v)); }
double F(std::uint64_t v) { return std::bit_cast<double>(v); }
std::uint64_t Bits(double v) { return std::bit_cast<std::uint64_t>(v); }
std::uint64_t Single(double v) { return Bits(double(float(v))); }
void CmpS(Registers& s, std::uint64_t x, std::uint64_t y)
{
    s.cr6 = {std::uint8_t(S(x) < S(y)), std::uint8_t(S(x) > S(y)),
        std::uint8_t(S(x) == S(y)), s.xer_so};
}
void CmpU(Registers& s, std::uint64_t x, std::uint64_t y)
{
    s.cr6 = {std::uint8_t(W(x) < W(y)), std::uint8_t(W(x) > W(y)),
        std::uint8_t(W(x) == W(y)), s.xer_so};
}
void CmpF(Registers& s, std::uint64_t x, std::uint64_t y)
{
    const auto a = F(x), b = F(y);
    const bool un = std::isnan(a) || std::isnan(b);
    s.cr6 = {std::uint8_t(!un && a < b), std::uint8_t(!un && a > b),
        std::uint8_t(!un && a == b), std::uint8_t(un)};
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
std::uint64_t LoadF(GuestMemory& m, NativeServices& native,
    Registers& s, std::uint64_t address)
{
    DisableFlush(native, s);
    return LoadedSingle::FromWord(m.ReadU32(W(address))).FprBits();
}
void StoreF(GuestMemory& m, std::uint64_t address, std::uint64_t bits)
{ m.WriteU32(W(address), std::bit_cast<std::uint32_t>(float(F(bits)))); }

class LowerServices final : public object_curve_sample::NativeServices,
    public object_sample_accumulator::NativeServices
{
public:
    explicit LowerServices(object_curve_owner_mix::NativeServices& native)
        : native_(native) {}
    void SetHostFpControl(std::uint32_t control) override
    { native_.SetHostFpControl(control); }
    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_curve_sample::Registers& base) override
    { native_.CallVirtual(target, memory, static_cast<Registers&>(base)); }
private:
    object_curve_owner_mix::NativeServices& native_;
};

void Save28(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x82625c48u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 28u; i <= 31u; ++i)
        WriteU64(m, sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(sp - 8u, W(R(s, 12)));
    DisableFlush(native, s);
    WriteU64(m, sp - 64u, s.f29_bits);
    WriteU64(m, sp - 56u, s.f30_bits);
    WriteU64(m, sp - 48u, s.f31_bits);
    R(s, 1) -= 240u;
    m.WriteU32(W(R(s, 1)), sp);
}
void Restore28(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 1) += 240u;
    DisableFlush(native, s);
    const auto sp = W(R(s, 1));
    s.f29_bits = ReadU64(m, sp - 64u);
    s.f30_bits = ReadU64(m, sp - 56u);
    s.f31_bits = ReadU64(m, sp - 48u);
    for (unsigned i = 28u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
    s.lr = R(s, 12);
}

void SampleVector(GuestMemory& m, LowerServices& lower, Registers& s,
    std::uint32_t output, std::uint32_t return_pc)
{
    R(s, 3) = R(s, 1) + output;
    s.lr = return_pc;
    (void)object_curve_pair_apply::Apply(0x822c7fb8u, m, lower, s);
}
void SampleWrapper(GuestMemory& m, LowerServices& lower, Registers& s,
    std::uint32_t return_pc)
{
    s.lr = return_pc;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
}

void BlendVectors(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s, bool upper)
{
    if (upper)
    {
        R(s, 3) = R(s, 1) + 112u;
        s.f1_bits = LoadF(m, native, s, R(s, 31) + 140u);
        s.lr = 0x82625ce8u;
        (void)object_curve_pair_apply::Apply(0x822c7fb8u, m, lower, s);
    }
    else SampleVector(m, lower, s, 144u, 0x82625d90u);
    R(s, 7) = 0;
    R(s, 4) = R(s, 29) + 68u;
    R(s, 6) = m.ReadU32(W(R(s, 31) + 8u));
    s.f1_bits = LoadF(m, native, s,
        upper ? R(s, 31) + 140u : R(s, 30) + 12u);
    SampleVector(m, lower, s, upper ? 128u : 160u,
        upper ? 0x82625d00u : 0x82625da8u);

    R(s, 11) = 0xffffffff82190000ull;
    s.f11_bits = LoadF(m, native, s,
        R(s, 1) + (upper ? 128u : 160u));
    R(s, 3) = R(s, 28) + 96u;
    s.f13_bits = LoadF(m, native, s,
        R(s, 1) + (upper ? 116u : 148u));
    R(s, 5) = m.ReadU32(W(R(s, 31) + 8u));
    s.f10_bits = LoadF(m, native, s,
        R(s, 1) + (upper ? 132u : 164u));
    s.f13_bits = Single(F(s.f13_bits) * F(s.f31_bits));
    s.f12_bits = LoadF(m, native, s,
        R(s, 1) + (upper ? 120u : 152u));
    s.f0_bits = LoadF(m, native, s, R(s, 11) - 27252u);
    s.f12_bits = Single(F(s.f12_bits) * F(s.f31_bits));
    s.f30_bits = Single(F(s.f0_bits) - F(s.f31_bits));
    s.f0_bits = LoadF(m, native, s,
        R(s, 1) + (upper ? 112u : 144u));
    s.f0_bits = Single(F(s.f0_bits) * F(s.f31_bits));
    s.f9_bits = LoadF(m, native, s,
        R(s, 1) + (upper ? 136u : 168u));
    s.f1_bits = LoadF(m, native, s,
        upper ? R(s, 31) + 140u : R(s, 30) + 12u);
    s.f11_bits = Single(F(s.f11_bits) * F(s.f30_bits));
    s.f10_bits = Single(F(s.f10_bits) * F(s.f30_bits));
    s.f9_bits = Single(F(s.f9_bits) * F(s.f30_bits));
    s.f0_bits = Single(F(s.f0_bits) + F(s.f11_bits));
    StoreF(m, R(s, 1) + 96u, s.f0_bits);
    R(s, 11) = m.ReadU32(W(R(s, 1) + 96u));
    s.f0_bits = Single(F(s.f13_bits) + F(s.f10_bits));
    StoreF(m, R(s, 1) + 100u, s.f0_bits);
    s.f0_bits = Single(F(s.f12_bits) + F(s.f9_bits));
    StoreF(m, R(s, 1) + 104u, s.f0_bits);
    m.WriteU32(W(R(s, 1) + 80u), W(R(s, 11)));
    R(s, 11) = m.ReadU32(W(R(s, 1) + 100u));
    m.WriteU32(W(R(s, 1) + 84u), W(R(s, 11)));
    R(s, 11) = m.ReadU32(W(R(s, 1) + 104u));
    m.WriteU32(W(R(s, 1) + 88u), W(R(s, 11)));
    SampleWrapper(m, lower, s, upper ? 0x82625d7cu : 0x82625e24u);
    DisableFlush(native, s);
    s.f29_bits = s.f1_bits;
    s.f1_bits = LoadF(m, native, s,
        upper ? R(s, 31) + 140u : R(s, 30) + 12u);
}

void ApplyOwnerMix(GuestMemory& m, NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& s)
{
    LowerServices lower(native);
    Save28(m, native, s);
    R(s, 11) = 0xffffffff83310000ull;
    s.f31_bits = s.f2_bits;
    R(s, 28) = R(s, 3);
    R(s, 31) = R(s, 4);
    R(s, 30) = R(s, 7);
    R(s, 11) = m.ReadU32(W(R(s, 11) + 24228u));
    CmpS(s, R(s, 11), 1u);
    if (!s.cr6.eq)
    {
        R(s, 3) = m.ReadU32(W(R(s, 31) + 4u));
        s.lr = 0x82625c80u;
        (void)object_sample_accumulator::Apply(0x82607318u,
            m, lower, s);
    }
    R(s, 3) = R(s, 30);
    s.lr = 0x82625c88u;
    (void)object_blended_curve_apply::Apply(0x8262c430u, m,
        native, manager_services, registration_services, s);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 124u));
    R(s, 9) = m.ReadU32(W(R(s, 31) + 60u));
    R(s, 29) = R(s, 3);
    R(s, 11) = (std::uint64_t(W(R(s, 11))) << 1u) & 0xfffffffeu;
    R(s, 8) = m.ReadU32(W(R(s, 31) + 120u));
    R(s, 10) = m.ReadU32(W(R(s, 31) + 56u));
    R(s, 11) = m.ReadU16(W(R(s, 11)) + W(R(s, 9)));
    R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
        std::int64_t(S(R(s, 8))));
    R(s, 30) = R(s, 11) + R(s, 10);
    R(s, 11) = 0xffffffff82000000ull;
    s.f1_bits = LoadF(m, native, s, R(s, 30) + 12u);
    s.f0_bits = LoadF(m, native, s, R(s, 11) + 3664u);
    CmpF(s, s.f1_bits, s.f0_bits);
    if (!s.cr6.gt)
    {
        Restore28(m, native, s);
        return;
    }
    R(s, 11) = m.ReadU32(W(R(s, 28) + 124u));
    R(s, 7) = 0;
    R(s, 6) = m.ReadU32(W(R(s, 31) + 8u));
    R(s, 4) = R(s, 28) + 68u;
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    CmpU(s, R(s, 11), 0);
    BlendVectors(m, native, lower, s, !s.cr6.eq);

    R(s, 3) = R(s, 29) + 96u;
    R(s, 5) = m.ReadU32(W(R(s, 31) + 8u));
    s.lr = 0x82625e38u;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    s.f0_bits = Single(F(s.f29_bits) * F(s.f31_bits));
    s.f13_bits = LoadF(m, native, s, R(s, 30) + 96u);
    s.f12_bits = LoadF(m, native, s, R(s, 30) + 100u);
    s.f11_bits = LoadF(m, native, s, R(s, 1) + 80u);
    s.f10_bits = LoadF(m, native, s, R(s, 1) + 84u);
    s.f13_bits = Single(F(s.f13_bits) * F(s.f11_bits));
    s.f12_bits = Single(F(s.f12_bits) * F(s.f10_bits));
    s.f11_bits = LoadF(m, native, s, R(s, 30) + 104u);
    s.f10_bits = LoadF(m, native, s, R(s, 30) + 108u);
    s.f9_bits = LoadF(m, native, s, R(s, 1) + 88u);
    s.f11_bits = Single(F(s.f11_bits) * F(s.f9_bits));
    StoreF(m, R(s, 30) + 96u, s.f13_bits);
    StoreF(m, R(s, 30) + 100u, s.f12_bits);
    s.f0_bits = Single(std::fma(F(s.f1_bits), F(s.f30_bits),
        F(s.f0_bits)));
    StoreF(m, R(s, 30) + 104u, s.f11_bits);
    s.f0_bits = Single(F(s.f10_bits) * F(s.f0_bits));
    StoreF(m, R(s, 30) + 108u, s.f0_bits);
    Restore28(m, native, s);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state)
{
    if (entry != 0x82625c40u ||
        memory.ReadU32(0x833189e0u) == 0u)
        return false;
    ApplyOwnerMix(memory, native, manager_services,
        registration_services, state);
    return true;
}
} // namespace lo::semantic::gpu::object_curve_owner_mix
