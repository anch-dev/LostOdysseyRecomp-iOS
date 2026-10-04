#include "lo_semantics/object_curve_owner_mix.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_curve_owner_mix;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Object = 0x10000u, Params = 0x20000u;
constexpr GuestAddress Indices = 0x30000u, Records = 0x40000u;
constexpr GuestAddress CurveData = 0x50000u, Output = 0x60000u;
constexpr GuestAddress VirtualObject = 0x70000u, Vtable = 0x80000u;
constexpr GuestAddress Owner = 0x90000u, Singleton = 0xa0000u;
constexpr GuestAddress Other = 0xb0000u, Selectors = 0xc0000u;
constexpr GuestAddress NodeA = 0xd0000u, NodeB = 0xe0000u;
constexpr GuestAddress VirtualTarget = 0x82345680u;
constexpr GuestAddress BaseOne = 0x82000e50u;
constexpr GuestAddress BlendOne = 0x8218958cu;
constexpr GuestAddress Rng = 0x8331367cu;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x82189000u, 0x1000u},
    {0x83313000u, 0x1000u}, {0x83315000u, 0x5000u}};

enum class Mode { Early, OwnerMissEarly, Lower, Upper,
    Virtual, Selector, Alias };
constexpr Mode Cases[] = {Mode::Early, Mode::OwnerMissEarly,
    Mode::Lower, Mode::Upper, Mode::Virtual, Mode::Selector,
    Mode::Alias};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers s{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i) s.r[i] = fields[i]->u64;
    s.lr = c.lr; s.ctr = c.ctr.u64;
    s.f0_bits = c.f0.u64; s.f1_bits = c.f1.u64;
    s.f2_bits = c.f2.u64;
    s.f7_bits = c.f7.u64; s.f8_bits = c.f8.u64;
    s.f9_bits = c.f9.u64;
    s.f10_bits = c.f10.u64; s.f11_bits = c.f11.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.f29_bits = c.f29.u64;
    s.f30_bits = c.f30.u64; s.f31_bits = c.f31.u64;
    s.cached_fp_control = c.fpscr.csr;
    s.xer_so = c.xer.so; s.xer_ca = c.xer.ca;
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return s;
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i) fields[i]->u64 = s.r[i];
    c.lr = s.lr; c.ctr.u64 = s.ctr;
    c.f0.u64 = s.f0_bits; c.f1.u64 = s.f1_bits;
    c.f2.u64 = s.f2_bits;
    c.f7.u64 = s.f7_bits; c.f8.u64 = s.f8_bits;
    c.f9.u64 = s.f9_bits;
    c.f10.u64 = s.f10_bits; c.f11.u64 = s.f11_bits;
    c.f12.u64 = s.f12_bits; c.f13.u64 = s.f13_bits;
    c.f29.u64 = s.f29_bits;
    c.f30.u64 = s.f30_bits; c.f31.u64 = s.f31_bits;
    c.fpscr.csr = s.cached_fp_control;
    c.xer.so = s.xer_so; c.xer.ca = s.xer_ca;
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, {s.cr6.un}};
}

struct Event
{
    GuestAddress target;
    std::uint64_t r1, r3, r4, r5, r6, r7, lr, ctr;
    std::uint64_t f1, f2, f29, f31;
    bool operator==(const Event&) const = default;
};
struct Services final : family::NativeServices, ManagerFacadeServices,
    registered_constructor_family::RegistrationServices
{
    GuestMemory memory;
    std::vector<Event> events;
    explicit Services(test::GuestWindow& window) : memory(window.Memory()) {}
    void SetHostFpControl(std::uint32_t control) override
    { PPCFPSCRRegister{}.setcsr(control); }
    void CallVirtual(GuestAddress target, GuestMemory& guest,
        family::Registers& s) override
    {
        if (target != VirtualTarget)
            throw std::runtime_error("unexpected curve virtual target");
        events.push_back({target, s.r[1], s.r[3], s.r[4], s.r[5],
            s.r[6], s.r[7], s.lr, s.ctr, s.f1_bits, s.f2_bits,
            s.f29_bits, s.f31_bits});
        if (s.lr == 0x822c7fecu)
        {
            guest.WriteU32(Address(s.r[31]), 0x3fc00000u);
            guest.WriteU32(Address(s.r[31] + 4u), 0x40000000u);
            guest.WriteU32(Address(s.r[31] + 8u), 0x40200000u);
        }
        s.r[3] = 0x1234u;
        s.r[10] = 0xcafef00du;
        s.f1_bits = std::bit_cast<std::uint64_t>(1.25);
        s.f10_bits = std::bit_cast<std::uint64_t>(5.0);
        s.f11_bits = std::bit_cast<std::uint64_t>(-3.0);
        s.f12_bits = std::bit_cast<std::uint64_t>(2.0);
        s.f2_bits = std::bit_cast<std::uint64_t>(0.5);
        s.f29_bits = std::bit_cast<std::uint64_t>(0.75);
        s.f31_bits = std::bit_cast<std::uint64_t>(0.5);
    }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("cold manager allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("cold primary construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("cold fallback construction"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("cold manager method"); }
    std::uint64_t ReleaseStorage(GuestAddress,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("cold release"); }
    std::uint64_t AllocateStorage(GuestAddress,
        std::uint64_t, std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("cold storage allocation"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress,
        GuestAddress, std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("cold storage resize"); }
    std::uint64_t Register(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("cold registration"); }
};
Services* active = nullptr;
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};

void Curve(GuestMemory& m, GuestAddress address, bool dynamic)
{
    m.WriteU8(address + 1u, 1u);
    m.WriteU8(address + 2u, 1u);
    m.WriteU8(address + 3u, 1u);
    m.WriteU32(address + 4u, CurveData);
    m.WriteU32(address + 8u, 20u);
    m.WriteU32(address + 16u, 0x3f800000u);
    m.WriteU32(address + 20u, 0u);
    m.WriteU32(address + 24u, dynamic ? VirtualObject : 0u);
}
void Seed(GuestMemory& m, Mode mode)
{
    m.WriteU32(BaseOne, 0x3f800000u);
    m.WriteU32(0x82189798u, 0x3e800000u);
    m.WriteU32(BlendOne, 0x3f800000u);
    m.WriteU32(Rng, 7u);
    m.WriteU32(0x83315ea4u, mode == Mode::Selector ? 0u : 1u);
    m.WriteU32(0x833189e0u, Singleton);
    m.WriteU32(VirtualObject, Vtable);
    m.WriteU32(Vtable + 268u, VirtualTarget | 3u);
    Curve(m, Object + 68u, mode == Mode::Virtual);
    Curve(m, Object + 96u, mode == Mode::Virtual);
    Curve(m, Owner + 68u, mode == Mode::Virtual);
    Curve(m, Owner + 96u, mode == Mode::Virtual);
    m.WriteU32(Object + 124u,
        mode == Mode::Upper ? 0x80000000u : 0u);
    m.WriteU32(Owner + 52u,
        mode == Mode::OwnerMissEarly ? Other : Singleton);
    m.WriteU32(Object + 52u, Singleton);
    m.WriteU32(Other + 60u, 0u);
    m.WriteU32(Params + 4u, Params + 4u);
    m.WriteU32(Params + 8u, Output);
    m.WriteU32(Params + 56u, Records);
    m.WriteU32(Params + 60u, Indices);
    m.WriteU32(Params + 120u, 128u);
    m.WriteU32(Params + 124u, 0u);
    m.WriteU32(Params + 140u, 0x3fc00000u);
    m.WriteU32(Params + 220u, Selectors);
    m.WriteU32(Params + 224u, 2u);
    m.WriteU32(Selectors, NodeA);
    m.WriteU32(Selectors + 4u, NodeB);
    m.WriteU32(Selectors + 8u, 0x3f000000u);
    m.WriteU32(Output + 720u, 10u);
    m.WriteU32(NodeA + 60u, 2u);
    m.WriteU32(NodeA + 64u, 5u);
    m.WriteU32(NodeB + 64u, 15u);
    m.WriteU16(Indices, 0u);
    m.WriteU16(Indices + 2u, 1u);
    for (unsigned i = 0; i < 2u; ++i)
    {
        const auto record = Records + i * 128u;
        m.WriteU32(record + 12u,
            mode == Mode::Early || mode == Mode::OwnerMissEarly ?
                0x3f000000u : 0x3fc00000u);
        m.WriteU32(record + 92u, 0u);
        m.WriteU32(record + 96u, 0x40000000u);
        m.WriteU32(record + 100u, 0x40400000u);
        m.WriteU32(record + 104u, 0x40800000u);
        m.WriteU32(record + 108u, 0x40a00000u);
    }
    for (unsigned i = 0; i < 64u; ++i)
        m.WriteU32(CurveData + i * 4u,
            std::bit_cast<std::uint32_t>(float(i) * 0.25f));
}
PPCContext MakeContext(Mode mode)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1234000000000000ull + i * 0x100000001ull;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0xabcdef1200000000ull | Object;
    c.r4.u64 = 0x9876543200000000ull | Params;
    c.r5.u64 = Output;
    c.r7.u64 = 0x1111000000000000ull |
        (mode == Mode::Alias ? Object : Owner);
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    c.f1.u64 = std::bit_cast<std::uint64_t>(1.5);
    c.f2.u64 = std::bit_cast<std::uint64_t>(0.25);
    c.f7.u64 = std::bit_cast<std::uint64_t>(-7.5);
    c.f8.u64 = std::bit_cast<std::uint64_t>(-8.5);
    c.f9.u64 = std::bit_cast<std::uint64_t>(-9.5);
    c.f10.u64 = std::bit_cast<std::uint64_t>(-10.0);
    c.f11.u64 = std::bit_cast<std::uint64_t>(-11.0);
    c.f12.u64 = std::bit_cast<std::uint64_t>(-12.0);
    c.f13.u64 = std::bit_cast<std::uint64_t>(-13.0);
    c.f29.u64 = std::bit_cast<std::uint64_t>(-29.0);
    c.f30.u64 = std::bit_cast<std::uint64_t>(-30.0);
    c.f31.u64 = std::bit_cast<std::uint64_t>(-31.0);
    c.fpscr.csr = 0x9fc0u;
    c.xer.so = 1; c.xer.ca = 1;
    c.cr6 = {0, 1, 0, {1}};
    return c;
}

bool Compare(PPCContext& expected, const family::Registers& actual,
    const test::GuestWindow& original,
    const test::GuestWindow& recovered,
    const Services& a, const Services& b, unsigned ordinal)
{
    const auto observed = FromPpc(expected);
    bool same = original.EqualCommitted(recovered) && a.events == b.events;
    for (unsigned i = 0; i < 32u; ++i)
        if (observed.r[i] != actual.r[i])
        {
            std::fprintf(stderr, "case %u r%u %llx/%llx\n", ordinal, i,
                static_cast<unsigned long long>(observed.r[i]),
                static_cast<unsigned long long>(actual.r[i]));
            same = false;
        }
    auto check = [&](const char* label, std::uint64_t x, std::uint64_t y)
    {
        if (x == y) return;
        std::fprintf(stderr, "case %u %s %llx/%llx\n", ordinal, label,
            static_cast<unsigned long long>(x),
            static_cast<unsigned long long>(y));
        same = false;
    };
    check("lr", observed.lr, actual.lr);
    check("ctr", observed.ctr, actual.ctr);
    check("f0", observed.f0_bits, actual.f0_bits);
    check("f1", observed.f1_bits, actual.f1_bits);
    check("f2", observed.f2_bits, actual.f2_bits);
    check("f7", observed.f7_bits, actual.f7_bits);
    check("f8", observed.f8_bits, actual.f8_bits);
    check("f9", observed.f9_bits, actual.f9_bits);
    check("f10", observed.f10_bits, actual.f10_bits);
    check("f11", observed.f11_bits, actual.f11_bits);
    check("f12", observed.f12_bits, actual.f12_bits);
    check("f13", observed.f13_bits, actual.f13_bits);
    check("f29", observed.f29_bits, actual.f29_bits);
    check("f30", observed.f30_bits, actual.f30_bits);
    check("f31", observed.f31_bits, actual.f31_bits);
    check("fp-control", observed.cached_fp_control,
        actual.cached_fp_control);
    check("xer-so", observed.xer_so, actual.xer_so);
    check("xer-ca", observed.xer_ca, actual.xer_ca);
    check("cr-lt", observed.cr6.lt, actual.cr6.lt);
    check("cr-gt", observed.cr6.gt, actual.cr6.gt);
    check("cr-eq", observed.cr6.eq, actual.cr6.eq);
    check("cr-un", observed.cr6.un, actual.cr6.un);
    if (a.events != b.events)
        std::fprintf(stderr, "case %u events %zu/%zu\n", ordinal,
            a.events.size(), b.events.size());
    if (!original.EqualCommitted(recovered))
        for (const auto region : Regions)
            for (std::size_t i = 0; i < region.size; ++i)
                if (original.Bytes()[region.base + i] !=
                    recovered.Bytes()[region.base + i])
                {
                    std::fprintf(stderr, "case %u RAM %08llx %02x/%02x\n",
                        ordinal,
                        static_cast<unsigned long long>(region.base + i),
                        original.Bytes()[region.base + i],
                        recovered.Bytes()[region.base + i]);
                    break;
                }
    return same;
}
bool Check(Mode mode, unsigned ordinal)
{
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    original.Fill(0xbd); recovered.Fill(0xbd);
    Services expected(original), actual(recovered);
    Seed(expected.memory, mode); Seed(actual.memory, mode);
    auto context = MakeContext(mode);
    auto state = FromPpc(context);
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    constexpr GuestAddress entry = 0x82625c40u;
    __imp__sub_82625C40(context, original.Bytes());
    active = nullptr;
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(entry, actual.memory, actual,
            actual, actual, state))
        throw std::runtime_error("missing owner curve entry");
    const bool covered = (mode == Mode::Virtual ?
            expected.events.size() == 4u : expected.events.empty()) &&
        (mode != Mode::Selector ||
            expected.memory.ReadU32(Params + 16u) == NodeA) &&
        (mode == Mode::Early || mode == Mode::OwnerMissEarly ||
            expected.memory.ReadU32(Records + 96u) != 0x40000000u);
    if (!covered)
        std::fprintf(stderr, "case %u missed owner curve path\n", ordinal);
    return covered && Compare(context, state, original, recovered,
        expected, actual, ordinal);
}
} // namespace

void OriginalSave28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u), fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalSave27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u), fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalIndirect(std::uint32_t target, PPCContext& c, std::uint8_t*)
{
    auto state = FromPpc(c);
    active->CallVirtual(target, active->memory, state);
    ToPpc(c, state);
}
void OriginalColdCall(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("cold singleton original-side path"); }

int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-curve-owner-mix %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 82625C40/8262C430/8242CC78/82607318/822C7FB8/822C7388/822C73E8/822C78D8 bodies; warm singleton, vtable+268 selected-context boundary, finite normal FP; cold construction, faults, MMIO and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
