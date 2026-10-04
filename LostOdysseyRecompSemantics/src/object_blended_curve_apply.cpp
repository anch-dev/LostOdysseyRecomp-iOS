#include "lo_semantics/object_blended_curve_apply.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_blended_curve_apply
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

class LowerServices final : public object_curve_pair_apply::NativeServices,
    public object_sample_accumulator::NativeServices
{
public:
    explicit LowerServices(object_blended_curve_apply::NativeServices& native)
        : native_(native) {}
    void SetHostFpControl(std::uint32_t control) override
    { native_.SetHostFpControl(control); }
    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_curve_sample::Registers& base) override
    { native_.CallVirtual(target, memory, static_cast<Registers&>(base)); }
private:
    object_blended_curve_apply::NativeServices& native_;
};

void Save24(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 12) = s.lr;
    s.lr = 0x82625e98u;
    const auto old_sp = W(R(s, 1));
    for (unsigned i = 24u; i <= 31u; ++i)
        WriteU64(m, old_sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    DisableFlush(native, s);
    WriteU64(m, old_sp - 96u, s.f29_bits);
    WriteU64(m, old_sp - 88u, s.f30_bits);
    WriteU64(m, old_sp - 80u, s.f31_bits);
    R(s, 1) -= 304u;
    m.WriteU32(W(R(s, 1)), old_sp);
}
void Restore24(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 1) += 304u;
    DisableFlush(native, s);
    const auto sp = W(R(s, 1));
    s.f29_bits = ReadU64(m, sp - 96u);
    s.f30_bits = ReadU64(m, sp - 88u);
    s.f31_bits = ReadU64(m, sp - 80u);
    for (unsigned i = 24u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
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
    R(s, 3) = m.ReadU32(0x833189e0u);
    CmpU(s, R(s, 3), 0);
    std::uint64_t result = 0;
    (void)registered_constructor_family::Apply(0x8242cc78u, m,
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
    R(s, 12) = s.lr;
    const auto old_sp = W(R(s, 1));
    m.WriteU32(old_sp - 8u, W(R(s, 12)));
    WriteU64(m, old_sp - 16u, R(s, 31));
    R(s, 1) -= 96u;
    m.WriteU32(W(R(s, 1)), old_sp);
    R(s, 31) = R(s, 3);
    CmpU(s, R(s, 31), 0);
    if (!s.cr6.eq)
    {
        s.lr = 0x8262c450u;
        GetSingleton(m, s, manager_services, registration_services);
        R(s, 11) = m.ReadU32(W(R(s, 31) + 52u));
        CmpU(s, R(s, 11), 0);
        while (!s.cr6.eq)
        {
            CmpU(s, R(s, 11), R(s, 3));
            if (s.cr6.eq)
            {
                R(s, 3) = R(s, 31);
                goto done;
            }
            R(s, 11) = m.ReadU32(W(R(s, 11) + 60u));
            CmpU(s, R(s, 11), 0);
        }
        R(s, 11) = W(R(s, 3)) == 0 ? 1u : 0u;
        CmpS(s, R(s, 11), 0);
        if (!s.cr6.eq)
        {
            R(s, 3) = R(s, 31);
            goto done;
        }
    }
    R(s, 3) = 0;
done:
    R(s, 1) += 96u;
    R(s, 12) = m.ReadU32(W(R(s, 1) - 8u));
    s.lr = R(s, 12);
    R(s, 31) = ReadU64(m, W(R(s, 1) - 16u));
}

void SampleVector(GuestMemory& m, LowerServices& lower, Registers& s,
    std::uint32_t offset, std::uint32_t return_pc)
{
    R(s, 3) = R(s, 1) + offset;
    s.lr = return_pc;
    (void)object_curve_pair_apply::Apply(0x822c7fb8u, m, lower, s);
}
void SampleScalar(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s, std::uint32_t output,
    std::uint32_t virtual_pc, std::uint32_t direct_pc, bool keep_f30)
{
    R(s, 11) = m.ReadU32(W(R(s, 30) + 24u));
    CmpU(s, R(s, 11), 0);
    if (!s.cr6.eq)
    {
        R(s, 3) = W(R(s, 11));
        R(s, 11) = m.ReadU32(W(R(s, 3)));
        R(s, 11) = m.ReadU32(W(R(s, 11) + 268u));
        s.ctr = R(s, 11);
        s.lr = virtual_pc;
        native.CallVirtual(W(s.ctr) & ~3u, m, s);
        if (keep_f30)
        {
            DisableFlush(native, s);
            s.f30_bits = s.f1_bits;
        }
    }
    else
    {
        R(s, 7) = 0;
        R(s, 6) = 1;
        R(s, 5) = R(s, 1) + output;
        R(s, 3) = R(s, 30);
        s.lr = direct_pc;
        (void)object_curve_sample::Apply(0x822c73e8u, m, lower, s);
        if (keep_f30)
            s.f30_bits = LoadF(m, native, s, R(s, 1) + output);
        else
            s.f1_bits = LoadF(m, native, s, R(s, 1) + output);
    }
}

void BlendRecord(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s, bool upper)
{
    const auto time = upper ? R(s, 31) + 140u : R(s, 29) + 12u;
    const auto vector1 = upper ? 144u : 176u;
    const auto vector2 = upper ? 160u : 192u;
    s.f1_bits = LoadF(m, native, s, time);
    SampleVector(m, lower, s, vector1,
        upper ? 0x82625f4cu : 0x8262606cu);
    R(s, 7) = 0;
    R(s, 4) = R(s, 30);
    R(s, 6) = m.ReadU32(W(R(s, 31) + 8u));
    s.f1_bits = LoadF(m, native, s, time);
    SampleVector(m, lower, s, vector2,
        upper ? 0x82625f64u : 0x82626084u);
    R(s, 30) = R(s, 28) + 96u;
    R(s, 5) = m.ReadU32(W(R(s, 31) + 8u));
    s.f1_bits = LoadF(m, native, s, time);
    SampleScalar(m, native, lower, s, upper ? 80u : 88u,
        upper ? 0x82625f90u : 0x826260b0u,
        upper ? 0x82625facu : 0x826260ccu, true);

    R(s, 11) = m.ReadU32(W(R(s, 30) + 24u));
    s.f1_bits = LoadF(m, native, s, time);
    R(s, 5) = m.ReadU32(W(R(s, 31) + 8u));
    CmpU(s, R(s, 11), 0);
    if (!s.cr6.eq)
    {
        R(s, 3) = W(R(s, 11));
        R(s, 11) = m.ReadU32(W(R(s, 3)));
        R(s, 11) = m.ReadU32(W(R(s, 11) + 268u));
        s.ctr = R(s, 11);
        s.lr = upper ? 0x82625fd8u : 0x826260f8u;
        native.CallVirtual(W(s.ctr) & ~3u, m, s);
    }
    else
    {
        R(s, 7) = 0;
        R(s, 6) = 1;
        R(s, 5) = R(s, 1) + (upper ? 84u : 92u);
        R(s, 3) = R(s, 30);
        s.lr = upper ? 0x82625ff0u : 0x82626110u;
        (void)object_curve_sample::Apply(0x822c73e8u, m, lower, s);
        s.f1_bits = LoadF(m, native, s,
            R(s, 1) + (upper ? 84u : 92u));
    }

    s.f0_bits = Single(F(s.f29_bits) - F(s.f31_bits));
    s.f13_bits = LoadF(m, native, s, R(s, 1) + vector1);
    s.f12_bits = Single(F(s.f13_bits) * F(s.f31_bits));
    s.f13_bits = LoadF(m, native, s, R(s, 1) + vector1 + 4u);
    s.f9_bits = LoadF(m, native, s, R(s, 1) + vector2);
    s.f11_bits = Single(F(s.f13_bits) * F(s.f31_bits));
    s.f13_bits = LoadF(m, native, s, R(s, 1) + vector1 + 8u);
    s.f10_bits = Single(F(s.f13_bits) * F(s.f31_bits));
    s.f8_bits = LoadF(m, native, s, R(s, 1) + vector2 + 4u);
    s.f13_bits = Single(F(s.f30_bits) * F(s.f31_bits));
    s.f7_bits = LoadF(m, native, s, R(s, 1) + vector2 + 8u);
    s.f9_bits = Single(F(s.f9_bits) * F(s.f0_bits));
    s.f8_bits = Single(F(s.f8_bits) * F(s.f0_bits));
    s.f13_bits = Single(std::fma(F(s.f0_bits), F(s.f1_bits),
        F(s.f13_bits)));
    s.f0_bits = Single(F(s.f7_bits) * F(s.f0_bits));
    s.f12_bits = Single(F(s.f12_bits) + F(s.f9_bits));
    StoreF(m, R(s, 1) + (upper ? 112u : 128u), s.f12_bits);
    R(s, 11) = m.ReadU32(W(R(s, 1) + (upper ? 112u : 128u)));
    s.f12_bits = Single(F(s.f11_bits) + F(s.f8_bits));
    StoreF(m, R(s, 1) + (upper ? 116u : 132u), s.f12_bits);
    m.WriteU32(W(R(s, 1) + 96u), W(R(s, 11)));
    s.f0_bits = Single(F(s.f10_bits) + F(s.f0_bits));
    R(s, 11) = m.ReadU32(W(R(s, 1) + (upper ? 116u : 132u)));
    StoreF(m, R(s, 1) + (upper ? 120u : 136u), s.f0_bits);
    m.WriteU32(W(R(s, 1) + 100u), W(R(s, 11)));
    R(s, 11) = m.ReadU32(W(R(s, 1) + (upper ? 120u : 136u)));

    s.f0_bits = LoadF(m, native, s, R(s, 29) + 96u);
    m.WriteU32(W(R(s, 1) + 104u), W(R(s, 11)));
    s.f12_bits = LoadF(m, native, s, R(s, 29) + 100u);
    s.f11_bits = LoadF(m, native, s, R(s, 1) + 96u);
    s.f10_bits = LoadF(m, native, s, R(s, 1) + 100u);
    s.f0_bits = Single(F(s.f11_bits) * F(s.f0_bits));
    s.f12_bits = Single(F(s.f10_bits) * F(s.f12_bits));
    s.f11_bits = LoadF(m, native, s, R(s, 29) + 104u);
    s.f10_bits = LoadF(m, native, s, R(s, 29) + 108u);
    s.f9_bits = LoadF(m, native, s, R(s, 1) + 104u);
    s.f13_bits = Single(F(s.f13_bits) * F(s.f10_bits));
    s.f11_bits = Single(F(s.f9_bits) * F(s.f11_bits));
    StoreF(m, R(s, 29) + 96u, s.f0_bits);
    StoreF(m, R(s, 29) + 100u, s.f12_bits);
    StoreF(m, R(s, 29) + 104u, s.f11_bits);
    StoreF(m, R(s, 29) + 108u, s.f13_bits);
}

void ApplyBlended(GuestMemory& m, NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& s)
{
    LowerServices lower(native);
    Save24(m, native, s);
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
        s.lr = 0x82625ed0u;
        (void)object_sample_accumulator::Apply(0x82607318u,
            m, lower, s);
    }
    R(s, 3) = R(s, 30);
    s.lr = 0x82625ed8u;
    SelectOwner(m, s, manager_services, registration_services);
    R(s, 11) = m.ReadU32(W(R(s, 31) + 124u));
    R(s, 25) = m.ReadU32(W(R(s, 31) + 56u));
    R(s, 26) = R(s, 11) - 1u;
    R(s, 24) = m.ReadU32(W(R(s, 31) + 120u));
    R(s, 11) = m.ReadU32(W(R(s, 31) + 60u));
    CmpS(s, R(s, 26), 0);
    if (s.cr6.lt)
    {
        Restore24(m, native, s);
        return;
    }
    R(s, 10) = (std::uint64_t(W(R(s, 26))) << 1u) & 0xfffffffeu;
    R(s, 27) = R(s, 10) + R(s, 11);
    R(s, 11) = 0xffffffff82190000ull;
    s.f29_bits = LoadF(m, native, s, R(s, 11) - 27252u);
    for (;;)
    {
        R(s, 11) = m.ReadU16(W(R(s, 27)));
        R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
            std::int64_t(S(R(s, 24))));
        R(s, 29) = R(s, 11) + R(s, 25);
        R(s, 11) = m.ReadU32(W(R(s, 29) + 92u));
        R(s, 11) = W(R(s, 11)) & 1u;
        CmpS(s, R(s, 11), 0);
        if (s.cr6.eq)
        {
            R(s, 11) = m.ReadU32(W(R(s, 28) + 124u));
            R(s, 30) = R(s, 28) + 68u;
            R(s, 6) = m.ReadU32(W(R(s, 31) + 8u));
            R(s, 7) = 0;
            R(s, 11) = W(R(s, 11)) & 0x80000000u;
            R(s, 4) = R(s, 30);
            CmpU(s, R(s, 11), 0);
            BlendRecord(m, native, lower, s, !s.cr6.eq);
        }
        R(s, 26) -= 1u;
        R(s, 27) -= 2u;
        CmpS(s, R(s, 26), 0);
        if (s.cr6.lt) break;
    }
    Restore24(m, native, s);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state)
{
    if (entry != 0x82625e90u && entry != 0x8262c430u)
        return false;
    // Cold singleton residual scratch registers are outside this selected ABI.
    if (memory.ReadU32(0x833189e0u) == 0u)
        return false;
    if (entry == 0x82625e90u)
        ApplyBlended(memory, native, manager_services,
            registration_services, state);
    else
        SelectOwner(memory, state, manager_services, registration_services);
    return true;
}
} // namespace lo::semantic::gpu::object_blended_curve_apply
