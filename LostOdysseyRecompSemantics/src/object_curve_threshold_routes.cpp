#include "lo_semantics/object_curve_threshold_routes.h"

#include "lo_semantics/loaded_single.h"
#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/recovery_abi.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace lo::semantic::gpu::object_curve_threshold_routes
{
namespace
{
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
std::uint64_t& R(Registers& s, unsigned i) { return s.r[i]; }
std::uint32_t W(std::uint64_t x) { return Address(x); }
std::int32_t S(std::uint64_t x)
{ return std::bit_cast<std::int32_t>(W(x)); }
double F(std::uint64_t x) { return std::bit_cast<double>(x); }
std::uint64_t Bits(double x) { return std::bit_cast<std::uint64_t>(x); }
std::uint64_t Single(double x) { return Bits(double(float(x))); }
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
void CmpF(Registers& s, std::uint64_t a, std::uint64_t b)
{
    const auto x = F(a), y = F(b);
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
    explicit LowerServices(object_curve_threshold_routes::NativeServices& native)
        : native_(native) {}
    void SetHostFpControl(std::uint32_t control) override
    { native_.SetHostFpControl(control); }
    void CallVirtual(GuestAddress target, GuestMemory& memory,
        object_curve_sample::Registers& state) override
    { native_.CallVirtual(target, memory, static_cast<Registers&>(state)); }
private:
    object_curve_threshold_routes::NativeServices& native_;
};

void Save25(GuestMemory& m, NativeServices& native, Registers& s,
    std::uint32_t entry)
{
    R(s, 12) = s.lr;
    s.lr = entry + 8u;
    const auto sp = W(R(s, 1));
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(m, sp - 16u - (31u - i) * 8u, R(s, i));
    m.WriteU32(sp - 8u, W(R(s, 12)));
    DisableFlush(native, s);
    WriteU64(m, sp - 80u, s.f30_bits);
    WriteU64(m, sp - 72u, s.f31_bits);
    R(s, 1) -= 192u;
    m.WriteU32(W(R(s, 1)), sp);
}
void Restore25(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 1) += 192u;
    DisableFlush(native, s);
    const auto sp = W(R(s, 1));
    s.f30_bits = ReadU64(m, sp - 80u);
    s.f31_bits = ReadU64(m, sp - 72u);
    for (unsigned i = 25u; i <= 31u; ++i)
        R(s, i) = ReadU64(m, sp - 16u - (31u - i) * 8u);
    R(s, 12) = m.ReadU32(sp - 8u);
    s.lr = R(s, 12);
}
void GetSingleton(GuestMemory& m, Registers& s,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services)
{
    R(s, 12) = s.lr;
    const auto sp = W(R(s, 1));
    m.WriteU32(sp - 8u, W(R(s, 12)));
    WriteU64(m, sp - 16u, R(s, 31));
    R(s, 1) -= 128u;
    m.WriteU32(W(R(s, 1)), sp);
    R(s, 31) = 0xffffffff83320000ull;
    R(s, 3) = m.ReadU32(0x833189e8u);
    CmpU(s, R(s, 3), 0);
    std::uint64_t result = 0;
    (void)registered_constructor_family::Apply(0x8242ceb8u, m,
        manager_services, registration_services, R(s, 3), sp, result);
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
    R(s, 25) = m.ReadU32(W(R(s, 31) + 16u));
    CmpU(s, R(s, 30), 0);
    if (s.cr6.eq) { R(s, 30) = 0; return; }
    s.lr = 0x82626eacu;
    GetSingleton(m, s, manager_services, registration_services);
    R(s, 11) = m.ReadU32(W(R(s, 30) + 52u));
    CmpU(s, R(s, 11), 0);
    while (!s.cr6.eq)
    {
        CmpU(s, R(s, 11), R(s, 3));
        if (s.cr6.eq) return;
        R(s, 11) = m.ReadU32(W(R(s, 11) + 60u));
        CmpU(s, R(s, 11), 0);
    }
    R(s, 11) = W(R(s, 3)) == 0u ? 32u : 0u;
    R(s, 11) = std::rotl(std::uint64_t(W(R(s, 11))) |
        (R(s, 11) << 32u), 27) & 1u;
    CmpS(s, R(s, 11), 0);
    if (s.cr6.eq) R(s, 30) = 0;
}
void TransformPoint(GuestMemory& m, NativeServices& native, Registers& s)
{
    R(s, 11) = m.ReadU32(W(R(s, 31) + 8u));
    const auto base = R(s, 11);
    s.f12_bits = LoadF(m, native, s, R(s, 1) + 88u);
    s.f13_bits = LoadF(m, native, s, R(s, 1) + 80u);
    s.f0_bits = LoadF(m, native, s, R(s, 1) + 84u);
    s.f11_bits = LoadF(m, native, s, base + 144u);
    s.f11_bits = Single(F(s.f11_bits) * F(s.f12_bits));
    s.f10_bits = LoadF(m, native, s, base + 116u);
    s.f10_bits = Single(F(s.f10_bits) * F(s.f13_bits));
    s.f9_bits = LoadF(m, native, s, base + 120u);
    s.f8_bits = LoadF(m, native, s, base + 128u);
    s.f9_bits = Single(F(s.f9_bits) * F(s.f13_bits));
    s.f7_bits = LoadF(m, native, s, base + 132u);
    s.f6_bits = LoadF(m, native, s, base + 136u);
    s.f5_bits = LoadF(m, native, s, base + 112u);
    s.f4_bits = LoadF(m, native, s, base + 148u);
    s.f3_bits = LoadF(m, native, s, base + 152u);
    s.f2_bits = LoadF(m, native, s, base + 160u);
    s.f1_bits = LoadF(m, native, s, base + 164u);
    s.f11_bits = Single(std::fma(F(s.f8_bits), F(s.f0_bits), F(s.f11_bits)));
    s.f8_bits = LoadF(m, native, s, base + 168u);
    s.f10_bits = Single(std::fma(F(s.f7_bits), F(s.f0_bits), F(s.f10_bits)));
    s.f0_bits = Single(std::fma(F(s.f6_bits), F(s.f0_bits), F(s.f9_bits)));
    s.f13_bits = Single(std::fma(F(s.f13_bits), F(s.f5_bits), F(s.f11_bits)));
    s.f11_bits = Single(std::fma(F(s.f4_bits), F(s.f12_bits), F(s.f10_bits)));
    s.f12_bits = Single(std::fma(F(s.f3_bits), F(s.f12_bits), F(s.f0_bits)));
    s.f0_bits = Single(std::fma(F(s.f2_bits), F(s.f31_bits), F(s.f13_bits)));
    StoreF(m, R(s, 1) + 96u, s.f0_bits);
    R(s, 11) = m.ReadU32(W(R(s, 1) + 96u));
    s.f13_bits = Single(std::fma(F(s.f1_bits), F(s.f31_bits), F(s.f11_bits)));
    StoreF(m, R(s, 1) + 100u, s.f13_bits);
    s.f12_bits = Single(std::fma(F(s.f8_bits), F(s.f31_bits), F(s.f12_bits)));
    StoreF(m, R(s, 1) + 104u, s.f12_bits);
    m.WriteU32(W(R(s, 1) + 80u), W(R(s, 11)));
    R(s, 11) = m.ReadU32(W(R(s, 1) + 100u));
    m.WriteU32(W(R(s, 1) + 84u), W(R(s, 11)));
    R(s, 11) = m.ReadU32(W(R(s, 1) + 104u));
    m.WriteU32(W(R(s, 1) + 88u), W(R(s, 11)));
}
void TestRecords(GuestMemory& m, NativeServices& native, Registers& s,
    std::uint32_t virtual_pc)
{
    R(s, 11) = m.ReadU32(W(R(s, 31) + 124u));
    R(s, 28) = m.ReadU32(W(R(s, 31) + 56u));
    R(s, 30) = R(s, 11) - 1u;
    R(s, 27) = m.ReadU32(W(R(s, 31) + 120u));
    R(s, 11) = m.ReadU32(W(R(s, 31) + 60u));
    CmpS(s, R(s, 30), 0);
    if (s.cr6.lt) return;
    R(s, 10) = (std::uint64_t(W(R(s, 30))) << 1u) & 0xfffffffeu;
    R(s, 29) = R(s, 10) + R(s, 11);
    R(s, 11) = 0xffffffff82000000ull;
    s.f31_bits = LoadF(m, native, s, R(s, 11) + 3664u);
    for (;;)
    {
        R(s, 11) = m.ReadU16(W(R(s, 29)));
        R(s, 11) = std::uint64_t(std::int64_t(S(R(s, 11))) *
            std::int64_t(S(R(s, 27))));
        R(s, 11) += R(s, 28);
        R(s, 10) = m.ReadU32(W(R(s, 11) + 92u));
        R(s, 10) = W(R(s, 10)) & 1u;
        CmpS(s, R(s, 10), 0);
        if (s.cr6.eq)
        {
            R(s, 11) += 16u;
            R(s, 10) = m.ReadU32(W(R(s, 25) + 72u));
            R(s, 9) = m.ReadU32(W(R(s, 11)));
            R(s, 10) = m.ReadU32(W(R(s, 10) + 68u));
            R(s, 10) = W(R(s, 10)) & 0x80000000u;
            m.WriteU32(W(R(s, 1) + 80u), W(R(s, 9)));
            CmpU(s, R(s, 10), 0);
            R(s, 9) = m.ReadU32(W(R(s, 11) + 4u));
            R(s, 11) = m.ReadU32(W(R(s, 11) + 8u));
            m.WriteU32(W(R(s, 1) + 84u), W(R(s, 9)));
            m.WriteU32(W(R(s, 1) + 88u), W(R(s, 11)));
            if (!s.cr6.eq) TransformPoint(m, native, s);
            R(s, 11) = m.ReadU32(W(R(s, 26) + 96u));
            s.f0_bits = LoadF(m, native, s, R(s, 1) + 88u);
            R(s, 11) = (W(R(s, 11)) >> 30u) & 1u;
            CmpU(s, R(s, 11), 1);
            bool call = false;
            if (s.cr6.eq)
            {
                CmpF(s, s.f0_bits, s.f30_bits);
                call = s.cr6.lt;
            }
            if (!call)
            {
                CmpU(s, R(s, 11), 0);
                if (s.cr6.eq)
                {
                    CmpF(s, s.f0_bits, s.f30_bits);
                    call = s.cr6.gt;
                }
            }
            if (call)
            {
                R(s, 11) = m.ReadU32(W(R(s, 31)));
                R(s, 3) = R(s, 31);
                R(s, 4) = R(s, 30);
                R(s, 11) = m.ReadU32(W(R(s, 11) + 108u));
                s.ctr = R(s, 11);
                s.lr = virtual_pc;
                native.CallVirtual(W(s.ctr) & ~3u, m, s);
            }
        }
        R(s, 30) -= 1u;
        R(s, 29) -= 2u;
        CmpS(s, R(s, 30), 0);
        if (s.cr6.lt) break;
    }
}
void Sample(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s, std::uint64_t curve,
    std::uint32_t return_pc)
{
    R(s, 3) = curve + 68u;
    R(s, 5) = m.ReadU32(W(R(s, 31) + 8u));
    s.f1_bits = LoadF(m, native, s, R(s, 31) + 140u);
    s.lr = return_pc;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
}
void Direct(GuestMemory& m, NativeServices& native,
    LowerServices& lower, Registers& s)
{
    R(s, 26) = R(s, 3);
    R(s, 31) = R(s, 4);
    R(s, 11) = m.ReadU32(0x83315ea4u);
    CmpS(s, R(s, 11), 1);
    if (!s.cr6.eq)
    {
        R(s, 3) = m.ReadU32(W(R(s, 31) + 4u));
        s.lr = 0x82626cbcu;
        (void)object_sample_accumulator::Apply(0x82607318u, m, lower, s);
    }
    R(s, 3) = R(s, 26) + 68u;
    R(s, 25) = m.ReadU32(W(R(s, 31) + 16u));
    R(s, 5) = m.ReadU32(W(R(s, 31) + 8u));
    s.f1_bits = LoadF(m, native, s, R(s, 31) + 140u);
    s.lr = 0x82626cd0u;
    (void)object_curve_sample::Apply(0x822c7388u, m, lower, s);
    R(s, 11) = m.ReadU32(W(R(s, 26) + 96u));
    DisableFlush(native, s);
    s.f30_bits = s.f1_bits;
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    CmpU(s, R(s, 11), 0);
    if (s.cr6.eq)
    {
        R(s, 11) = m.ReadU32(W(R(s, 31) + 8u));
        s.f0_bits = LoadF(m, native, s, R(s, 11) + 168u);
        s.f30_bits = Single(F(s.f0_bits) + F(s.f30_bits));
    }
    TestRecords(m, native, s, 0x82626e40u);
}
void Blended(GuestMemory& m, NativeServices& native,
    LowerServices& lower,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& s)
{
    DisableFlush(native, s);
    s.f31_bits = s.f2_bits;
    R(s, 26) = R(s, 3);
    R(s, 31) = R(s, 4);
    R(s, 30) = R(s, 7);
    R(s, 11) = m.ReadU32(0x83315ea4u);
    CmpS(s, R(s, 11), 1);
    if (!s.cr6.eq)
    {
        R(s, 3) = m.ReadU32(W(R(s, 31) + 4u));
        s.lr = 0x82626e9cu;
        (void)object_sample_accumulator::Apply(0x82607318u, m, lower, s);
    }
    SelectOwner(m, s, manager_services, registration_services);
    Sample(m, native, lower, s, R(s, 26), 0x82626ef0u);
    DisableFlush(native, s);
    s.f30_bits = s.f1_bits;
    Sample(m, native, lower, s, R(s, 30), 0x82626f04u);
    R(s, 11) = 0xffffffff82190000ull;
    s.f13_bits = Single(F(s.f30_bits) * F(s.f31_bits));
    s.f0_bits = LoadF(m, native, s, R(s, 11) - 27252u);
    R(s, 11) = m.ReadU32(W(R(s, 26) + 96u));
    s.f0_bits = Single(F(s.f0_bits) - F(s.f31_bits));
    R(s, 11) = W(R(s, 11)) & 0x80000000u;
    CmpU(s, R(s, 11), 0);
    s.f30_bits = Single(std::fma(F(s.f1_bits), F(s.f0_bits), F(s.f13_bits)));
    if (s.cr6.eq)
    {
        R(s, 11) = m.ReadU32(W(R(s, 31) + 8u));
        s.f0_bits = LoadF(m, native, s, R(s, 11) + 168u);
        s.f30_bits = Single(F(s.f0_bits) + F(s.f30_bits));
    }
    TestRecords(m, native, s, 0x82627084u);
}
} // namespace

bool Apply(GuestAddress entry, GuestMemory& memory,
    NativeServices& native,
    ManagerFacadeServices& manager_services,
    registered_constructor_family::RegistrationServices& registration_services,
    Registers& state)
{
    if (entry != 0x82626c88u && entry != 0x82626e60u) return false;
    if (entry == 0x82626e60u && W(state.r[7]) != 0u &&
        memory.ReadU32(0x833189e8u) == 0u)
        return false;
    LowerServices lower(native);
    Save25(memory, native, state, entry);
    if (entry == 0x82626c88u) Direct(memory, native, lower, state);
    else Blended(memory, native, lower, manager_services,
        registration_services, state);
    Restore25(memory, native, state);
    return true;
}
} // namespace lo::semantic::gpu::object_curve_threshold_routes
