#include "lo_semantics/object_curve_owner_node_record_update.h"
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
namespace family = object_curve_owner_node_record_update;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Object = 0x10000u, Params = 0x20000u;
constexpr GuestAddress Indices = 0x30000u, Records = 0x40000u;
constexpr GuestAddress CurveData = 0x50000u, Output = 0x60000u;
constexpr GuestAddress VirtualObject = 0x70000u, Vtable = 0x80000u;
constexpr GuestAddress ParamTransform = 0x90000u, NodeChild = 0xa0000u;
constexpr GuestAddress Node = 0xb0000u, NodeMetadata = 0xc0000u;
constexpr GuestAddress NodeData = 0xd0000u, NodeList = 0xe0000u;
constexpr GuestAddress ParamChild = 0xf0000u, NodeTransform = 0x100000u;
constexpr GuestAddress Owner = 0x104000u, Singleton = 0x106000u;
constexpr GuestAddress Other = 0x108000u;
constexpr GuestAddress VirtualTarget = 0x82345680u;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x82000000u, 0x4000u}, {0x82189000u, 0x1000u},
    {0x83313000u, 0x1000u}, {0x83315000u, 0x5000u}};
enum class Mode { NoObject, OwnerMiss, OwnerZero, LiteralForce,
    MismatchInvalidate, VirtualNull, HitSecondary, RadiusInterpolation,
    NodeTransform, RecordTransform };
constexpr Mode Cases[] = {Mode::NoObject, Mode::OwnerMiss,
    Mode::OwnerZero, Mode::LiteralForce, Mode::MismatchInvalidate,
    Mode::VirtualNull, Mode::HitSecondary, Mode::RadiusInterpolation,
    Mode::NodeTransform, Mode::RecordTransform};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
std::array<PPCRegister*, 32> FprFields(PPCContext& c)
{
    return {&c.f0, &c.f1, &c.f2, &c.f3, &c.f4, &c.f5, &c.f6, &c.f7,
        &c.f8, &c.f9, &c.f10, &c.f11, &c.f12, &c.f13, &c.f14, &c.f15,
        &c.f16, &c.f17, &c.f18, &c.f19, &c.f20, &c.f21, &c.f22, &c.f23,
        &c.f24, &c.f25, &c.f26, &c.f27, &c.f28, &c.f29, &c.f30, &c.f31};
}
std::array<std::uint64_t*, 32> FprFields(family::Registers& s)
{
    return {&s.f0_bits, &s.f1_bits, &s.f2_bits, &s.f3_bits,
        &s.f4_bits, &s.f5_bits, &s.f6_bits, &s.f7_bits, &s.f8_bits,
        &s.f9_bits, &s.f10_bits, &s.f11_bits, &s.f12_bits, &s.f13_bits,
        &s.f14_bits, &s.f15_bits, &s.f16_bits, &s.f17_bits, &s.f18_bits,
        &s.f19_bits, &s.f20_bits, &s.f21_bits, &s.f22_bits, &s.f23_bits,
        &s.f24_bits, &s.f25_bits, &s.f26_bits, &s.f27_bits, &s.f28_bits,
        &s.f29_bits, &s.f30_bits, &s.f31_bits};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers s{};
    const auto r = Fields(c), f = FprFields(c);
    const auto sf = FprFields(s);
    for (unsigned i = 0; i < 32u; ++i)
    { s.r[i] = r[i]->u64; *sf[i] = f[i]->u64; }
    s.lr = c.lr; s.ctr = c.ctr.u64;
    s.cached_fp_control = c.fpscr.csr;
    s.xer_so = c.xer.so; s.xer_ca = c.xer.ca;
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return s;
}
void ToPpc(PPCContext& c, family::Registers& s)
{
    const auto r = Fields(c), f = FprFields(c);
    const auto sf = FprFields(s);
    for (unsigned i = 0; i < 32u; ++i)
    { r[i]->u64 = s.r[i]; f[i]->u64 = *sf[i]; }
    c.lr = s.lr; c.ctr.u64 = s.ctr;
    c.fpscr.csr = s.cached_fp_control;
    c.xer.so = s.xer_so; c.xer.ca = s.xer_ca;
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, {s.cr6.un}};
}
std::array<std::uint64_t, 67> Snapshot(family::Registers s)
{
    std::array<std::uint64_t, 67> out{};
    const auto f = FprFields(s);
    for (unsigned i = 0; i < 32u; ++i)
    { out[i] = s.r[i]; out[i + 34u] = *f[i]; }
    out[32] = s.lr; out[33] = s.ctr;
    out[66] = s.cached_fp_control;
    return out;
}
struct Event
{
    GuestAddress target;
    std::array<std::uint64_t, 67> state;
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
    void CallVirtual(GuestAddress target, GuestMemory&,
        family::Registers& s) override
    {
        if (target != VirtualTarget || s.lr != 0x82629060u)
            throw std::runtime_error("unexpected record virtual call");
        events.push_back({target, Snapshot(s)});
        s.r[3] = current_mode == Mode::VirtualNull ? 0u : NodeData;
        s.r[10] = 0xcafef00du;
        s.f3_bits = std::bit_cast<std::uint64_t>(2.0);
        s.f10_bits = std::bit_cast<std::uint64_t>(5.0);
    }
    std::uint64_t AllocateRaw(std::uint32_t) override
    { throw std::runtime_error("cold allocation"); }
    std::uint64_t ConstructPrimary(std::uint64_t) override
    { throw std::runtime_error("cold construction"); }
    std::uint64_t ConstructFallback(std::uint64_t, GuestAddress) override
    { throw std::runtime_error("cold fallback"); }
    std::uint64_t CallMethod(GuestAddress, std::uint64_t) override
    { throw std::runtime_error("cold method"); }
    std::uint64_t ReleaseStorage(GuestAddress,
        std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("cold release"); }
    std::uint64_t AllocateStorage(GuestAddress,
        std::uint64_t, std::uint64_t, std::uint64_t) override
    { throw std::runtime_error("cold storage allocation"); }
    GuestAddress ResizeStorage(GuestAddress, GuestAddress,
        GuestAddress, std::uint32_t, std::uint32_t) override
    { throw std::runtime_error("cold resize"); }
    std::uint64_t Register(GuestAddress, std::uint64_t,
        GuestAddress) override
    { throw std::runtime_error("cold post callback"); }
    Mode current_mode = Mode::NoObject;
};
Services* active = nullptr;
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
void Curve(GuestMemory& m, GuestAddress address, GuestAddress data)
{
    m.WriteU8(address + 1u, 1u);
    m.WriteU8(address + 2u, 1u);
    m.WriteU8(address + 3u, 1u);
    m.WriteU32(address + 4u, data);
    m.WriteU32(address + 8u, 20u);
    m.WriteU32(address + 16u, 0x3f800000u);
    m.WriteU32(address + 20u, 0u);
    m.WriteU32(address + 24u, 0u);
}
void Seed(GuestMemory& m, Mode mode)
{
    m.WriteU32(0x8218958cu, 0x3f800000u);
    m.WriteU32(0x82189594u, 0x3f800000u);
    WriteU64(m, 0x82000f28u, std::bit_cast<std::uint64_t>(1.0));
    m.WriteU32(0x82000e50u, 0u);
    m.WriteU32(0x82003904u, 0x3f800000u);
    m.WriteU32(0x83315ea4u, 1u);
    m.WriteU32(0x833189f4u, Singleton);
    m.WriteU32(Owner + 52u,
        mode == Mode::OwnerMiss ? Other : Singleton);
    m.WriteU32(Other + 60u, 0u);
    m.WriteU32(Object + 68u, mode == Mode::NoObject ? 0u : 1u);
    m.WriteU32(Object + 72u, mode == Mode::NoObject ? 0u : 2u);
    m.WriteU32(Object + 104u,
        mode == Mode::LiteralForce ? 0x10000000u :
        mode == Mode::HitSecondary ? 0x40000000u :
        mode == Mode::RadiusInterpolation ? 0x80000000u : 0u);
    Curve(m, Object + 76u, CurveData);
    Curve(m, Object + 108u, CurveData + 0x100u);
    Curve(m, Owner + 76u, CurveData + 0x200u);
    Curve(m, Owner + 108u, CurveData + 0x300u);
    Curve(m, 76u, CurveData + 0x400u);
    Curve(m, 108u, CurveData + 0x500u);
    m.WriteU32(Params + 4u, Params + 4u);
    m.WriteU32(Params + 8u, Output);
    m.WriteU32(Params + 16u, ParamTransform);
    m.WriteU32(Params + 56u, Records);
    m.WriteU32(Params + 60u, Indices);
    m.WriteU32(Params + 120u, 256u);
    m.WriteU32(Params + 124u, 1u);
    m.WriteU32(Output + 628u, NodeList);
    m.WriteU32(Output + 632u, 1u);
    m.WriteU32(NodeList, Node);
    m.WriteU32(Node, Vtable);
    m.WriteU32(Node + 4u, NodeMetadata);
    m.WriteU32(Node + 16u, NodeTransform);
    m.WriteU32(Vtable + 116u, VirtualTarget | 3u);
    m.WriteU32(NodeMetadata + 60u, 1u);
    m.WriteU32(NodeMetadata + 64u, 2u);
    m.WriteU32(ParamTransform + 72u, ParamChild);
    m.WriteU32(NodeTransform + 72u, NodeChild);
    m.WriteU32(ParamChild + 68u,
        mode == Mode::RecordTransform ? 0x40000000u : 0u);
    m.WriteU32(NodeChild + 68u,
        mode == Mode::NodeTransform ? 0x40000000u : 0u);
    m.WriteU16(Indices, 0u);
    m.WriteU32(Records + 92u, 0u);
    m.WriteU32(Records + 16u, 0x40a00000u);
    m.WriteU32(Records + 20u, 0u);
    m.WriteU32(Records + 24u, 0u);
    for (const auto offset : {32u, 36u, 40u, 48u, 52u, 56u})
        m.WriteU32(Records + offset, 0u);
    m.WriteU32(Records + 128u,
        mode == Mode::LiteralForce ? 0xffffffffu : 0u);
    m.WriteU32(Records + 132u,
        mode == Mode::MismatchInvalidate ? ParamChild : 0u);
    m.WriteU32(Records + 136u, 0x3f800000u);
    m.WriteU32(Records + 140u, 0x40000000u);
    m.WriteU32(Records + 144u, 0x40400000u);
    m.WriteU32(NodeData + 12u, 0x3f000000u);
    m.WriteU32(NodeData + 16u, 0x40a00000u);
    m.WriteU32(NodeData + 20u, 0x3f800000u);
    m.WriteU32(NodeData + 24u, 0u);
    m.WriteU32(NodeData + 48u, 0x3f800000u);
    m.WriteU32(NodeData + 52u, 0x40000000u);
    m.WriteU32(NodeData + 56u, 0x40400000u);
    m.WriteU32(Output + 112u, 0x3f800000u);
    m.WriteU32(Output + 116u, 0u);
    m.WriteU32(Output + 120u, 0u);
    m.WriteU32(Output + 128u, 0u);
    m.WriteU32(Output + 132u, 0x3f800000u);
    m.WriteU32(Output + 136u, 0u);
    m.WriteU32(Output + 144u, 0u);
    m.WriteU32(Output + 148u, 0u);
    m.WriteU32(Output + 152u, 0x3f800000u);
    m.WriteU32(Output + 160u, 0u);
    m.WriteU32(Output + 164u, 0u);
    m.WriteU32(Output + 168u, 0u);
    m.WriteU32(Output + 720u, 10u);
    for (unsigned i = 0; i < 64u; ++i)
    {
        m.WriteU32(CurveData + i * 4u, 0x40000000u);
        m.WriteU32(CurveData + 0x100u + i * 4u, 0x3f800000u);
        m.WriteU32(CurveData + 0x200u + i * 4u, 0x40400000u);
        m.WriteU32(CurveData + 0x300u + i * 4u, 0x40000000u);
        m.WriteU32(CurveData + 0x400u + i * 4u, 0x40800000u);
        m.WriteU32(CurveData + 0x500u + i * 4u, 0x3f800000u);
    }
}
PPCContext MakeContext(Mode mode)
{
    PPCContext c{};
    const auto r = Fields(c), f = FprFields(c);
    for (unsigned i = 0; i < 32u; ++i)
    {
        r[i]->u64 = 0x1234000000000000ull + i * 0x100000001ull;
        f[i]->u64 = std::bit_cast<std::uint64_t>(-7.0);
    }
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0xabcdef1200000000ull | Object;
    c.r4.u64 = 0x9876543200000000ull | Params;
    c.r5.u64 = 0x5555000000000000ull | 128u;
    c.r7.u64 = mode == Mode::OwnerZero ? 0u :
        (0x2222000000000000ull | Owner);
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.f1.u64 = std::bit_cast<std::uint64_t>(1.25);
    c.f2.u64 = std::bit_cast<std::uint64_t>(0.4);
    c.fpscr.csr = 0x9fc0u;
    c.xer.so = 1; c.xer.ca = 1;
    c.cr6 = {0, 1, 0, {1}};
    return c;
}
bool Compare(PPCContext& expected, const family::Registers& actual,
    const test::GuestWindow& original, const test::GuestWindow& recovered,
    const Services& a, const Services& b, unsigned ordinal)
{
    const auto observed = FromPpc(expected);
    const auto x = Snapshot(observed), y = Snapshot(actual);
    bool same = original.EqualCommitted(recovered) && a.events == b.events;
    for (unsigned i = 0; i < x.size(); ++i)
        if (x[i] != y[i])
        {
            std::fprintf(stderr, "case %u state[%u] %llx/%llx\n", ordinal,
                i, static_cast<unsigned long long>(x[i]),
                static_cast<unsigned long long>(y[i]));
            same = false;
        }
    const auto check = [&](const char* label, std::uint64_t p,
        std::uint64_t q)
    {
        if (p == q) return;
        std::fprintf(stderr, "case %u %s %llx/%llx\n", ordinal, label,
            static_cast<unsigned long long>(p),
            static_cast<unsigned long long>(q));
        same = false;
    };
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
    expected.current_mode = mode;
    actual.current_mode = mode;
    Seed(expected.memory, mode); Seed(actual.memory, mode);
    auto context = MakeContext(mode);
    auto state = FromPpc(context);
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    __imp__sub_82628E50(context, original.Bytes());
    active = nullptr;
    const auto expected_events = mode == Mode::NoObject ||
        mode == Mode::LiteralForce ? 0u : 1u;
    bool covered = expected.events.size() == expected_events;
    if (mode == Mode::LiteralForce)
    {
        covered &= expected.memory.ReadU32(Records + 48u) == 0x3f800000u;
        covered &= expected.memory.ReadU32(Records + 52u) == 0x40000000u;
        covered &= expected.memory.ReadU32(Records + 56u) == 0x40400000u;
    }
    else if (mode == Mode::MismatchInvalidate)
    {
        covered &= expected.memory.ReadU32(Records + 128u) == 0xffffffffu;
        covered &= expected.memory.ReadU32(Records + 132u) == 0u;
    }
    else if (mode == Mode::OwnerMiss || mode == Mode::OwnerZero ||
        mode == Mode::HitSecondary ||
        mode == Mode::RadiusInterpolation ||
        mode == Mode::NodeTransform || mode == Mode::RecordTransform)
        covered &= expected.memory.ReadU32(Records + 52u) != 0u;
    else covered &= expected.memory.ReadU32(Records + 52u) == 0u;
    if (mode == Mode::HitSecondary)
        covered &= expected.memory.ReadU32(Records + 36u) != 0u;
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(0x82628E50u, actual.memory, actual, actual,
        actual, state))
        throw std::runtime_error("missing node record update entry");
    if (!covered)
        std::fprintf(stderr, "case %u missed requested record path\n", ordinal);
    return covered && Compare(context, state, original, recovered,
        expected, actual, ordinal);
}
} // namespace

void OriginalSave15(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 15u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u), r[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore15(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 15u; i <= 31u; ++i)
        r[i]->u64 = ReadU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalSaveF22(PPCContext& c, std::uint8_t*)
{
    const auto f = FprFields(c);
    for (unsigned i = 22u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r12.u64 - 80u +
            (i - 22u) * 8u), f[i]->u64);
}
void OriginalRestoreF22(PPCContext& c, std::uint8_t*)
{
    const auto f = FprFields(c);
    for (unsigned i = 22u; i <= 31u; ++i)
        f[i]->u64 = ReadU64(active->memory, Address(c.r12.u64 - 80u +
            (i - 22u) * 8u));
}
void OriginalSave27(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u), r[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        r[i]->u64 = ReadU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalColdCall(PPCContext&, std::uint8_t*)
{ throw std::runtime_error("cold singleton original-side path"); }
void OriginalIndirect(std::uint32_t target, PPCContext& c, std::uint8_t*)
{
    auto state = FromPpc(c);
    active->CallVirtual(target, active->memory, state);
    ToPpc(c, state);
}
int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-curve-owner-node-record-update %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT vtable+116 target, exceptional FP, faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
