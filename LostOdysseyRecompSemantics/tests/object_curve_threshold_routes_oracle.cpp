#include "lo_semantics/object_curve_threshold_routes.h"
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
namespace family = object_curve_threshold_routes;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Object = 0x10000u, Params = 0x20000u;
constexpr GuestAddress Indices = 0x30000u, Records = 0x40000u;
constexpr GuestAddress CurveData = 0x50000u, Output = 0x60000u;
constexpr GuestAddress VirtualObject = 0x70000u, Vtable = 0x80000u;
constexpr GuestAddress Owner = 0x90000u, Other = 0xa0000u;
constexpr GuestAddress Singleton = 0xe0000u;
constexpr GuestAddress TransformOwner = 0xf0000u;
constexpr GuestAddress Selectors = 0xb0000u, NodeA = 0xc0000u;
constexpr GuestAddress NodeB = 0xd0000u;
constexpr GuestAddress VirtualTarget = 0x82345680u;
constexpr GuestAddress BaseOne = 0x82000e50u;
constexpr GuestAddress BlendOne = 0x8218958cu;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x82189000u, 0x1000u},
    {0x83313000u, 0x1000u},
    {0x83315000u, 0x5000u}};
enum class Mode { DirectEmpty, DirectFlagged, DirectBelow,
    DirectAboveTransform, DirectVirtual, BlendedInside,
    BlendedOwnerMiss, BlendedSelector, BlendedTransform };
constexpr Mode Cases[] = {Mode::DirectEmpty, Mode::DirectFlagged,
    Mode::DirectBelow, Mode::DirectAboveTransform, Mode::DirectVirtual,
    Mode::BlendedInside, Mode::BlendedOwnerMiss, Mode::BlendedSelector,
    Mode::BlendedTransform};
bool Blended(Mode mode)
{ return mode == Mode::BlendedInside || mode == Mode::BlendedOwnerMiss ||
    mode == Mode::BlendedSelector || mode == Mode::BlendedTransform; }

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
    s.f2_bits = c.f2.u64; s.f3_bits = c.f3.u64;
    s.f4_bits = c.f4.u64; s.f5_bits = c.f5.u64;
    s.f6_bits = c.f6.u64; s.f7_bits = c.f7.u64;
    s.f8_bits = c.f8.u64; s.f9_bits = c.f9.u64;
    s.f10_bits = c.f10.u64; s.f11_bits = c.f11.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.f25_bits = c.f25.u64; s.f26_bits = c.f26.u64;
    s.f27_bits = c.f27.u64; s.f28_bits = c.f28.u64;
    s.f29_bits = c.f29.u64; s.f30_bits = c.f30.u64;
    s.f31_bits = c.f31.u64;
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
    c.f2.u64 = s.f2_bits; c.f3.u64 = s.f3_bits;
    c.f4.u64 = s.f4_bits; c.f5.u64 = s.f5_bits;
    c.f6.u64 = s.f6_bits; c.f7.u64 = s.f7_bits;
    c.f8.u64 = s.f8_bits; c.f9.u64 = s.f9_bits;
    c.f10.u64 = s.f10_bits; c.f11.u64 = s.f11_bits;
    c.f12.u64 = s.f12_bits; c.f13.u64 = s.f13_bits;
    c.f25.u64 = s.f25_bits; c.f26.u64 = s.f26_bits;
    c.f27.u64 = s.f27_bits; c.f28.u64 = s.f28_bits;
    c.f29.u64 = s.f29_bits; c.f30.u64 = s.f30_bits;
    c.f31.u64 = s.f31_bits;
    c.fpscr.csr = s.cached_fp_control;
    c.xer.so = s.xer_so; c.xer.ca = s.xer_ca;
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, {s.cr6.un}};
}
std::array<std::uint64_t, 56> Snapshot(const family::Registers& s)
{
    std::array<std::uint64_t, 56> result{};
    for (unsigned i = 0; i < 32u; ++i) result[i] = s.r[i];
    result[32] = s.lr; result[33] = s.ctr;
    const std::uint64_t fpr[] = {s.f0_bits, s.f1_bits, s.f2_bits,
        s.f3_bits, s.f4_bits, s.f5_bits, s.f6_bits, s.f7_bits,
        s.f8_bits, s.f9_bits, s.f10_bits, s.f11_bits, s.f12_bits,
        s.f13_bits, s.f25_bits, s.f26_bits, s.f27_bits, s.f28_bits,
        s.f29_bits, s.f30_bits, s.f31_bits};
    for (unsigned i = 0; i < 21u; ++i) result[34u + i] = fpr[i];
    result[55] = s.cached_fp_control;
    return result;
}
struct Event
{
    GuestAddress target;
    std::array<std::uint64_t, 56> state;
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
            throw std::runtime_error("unexpected threshold virtual target");
        events.push_back({target, Snapshot(s)});
        if (s.lr == 0x822c73b4u)
            s.f1_bits = std::bit_cast<std::uint64_t>(50.0);
        else
        {
            guest.WriteU32(Records + 112u, 0x12345678u);
            s.r[3] = 0x1234u;
            s.r[10] = 0xcafef00du;
            s.f1_bits = std::bit_cast<std::uint64_t>(1.25);
            s.f3_bits = std::bit_cast<std::uint64_t>(4.0);
        }
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
};
Services* active = nullptr;
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
void Curve(GuestMemory& m, GuestAddress address, GuestAddress data,
    bool dynamic)
{
    m.WriteU8(address + 1u, 1u);
    m.WriteU8(address + 2u, 1u);
    m.WriteU8(address + 3u, 1u);
    m.WriteU32(address + 4u, data);
    m.WriteU32(address + 8u, 20u);
    m.WriteU32(address + 16u, 0x3f800000u);
    m.WriteU32(address + 20u, 0u);
    m.WriteU32(address + 24u, dynamic ? VirtualObject : 0u);
}
void Seed(GuestMemory& m, Mode mode)
{
    m.WriteU32(BaseOne, 0x3f800000u);
    m.WriteU32(BlendOne, 0x3f800000u);
    m.WriteU32(0x833189e8u, Singleton);
    m.WriteU32(0x83315ea4u, mode == Mode::BlendedSelector ? 0u : 1u);
    Curve(m, Object + 68u, CurveData, mode == Mode::DirectVirtual);
    Curve(m, Owner + 68u, CurveData + 0x100u, false);
    Curve(m, 68u, CurveData + 0x200u, false);
    m.WriteU32(Object + 96u,
        mode == Mode::DirectAboveTransform ? 0u : 0x40000000u);
    m.WriteU32(Owner + 52u,
        mode == Mode::BlendedOwnerMiss ? Other : Singleton);
    m.WriteU32(Other + 60u, 0u);
    m.WriteU32(VirtualObject, Vtable);
    m.WriteU32(Vtable + 268u, VirtualTarget | 3u);
    m.WriteU32(Vtable + 108u, VirtualTarget | 3u);
    m.WriteU32(Params, Vtable);
    m.WriteU32(Params + 4u, Params + 4u);
    m.WriteU32(Params + 8u, Output);
    m.WriteU32(Params + 16u, TransformOwner);
    m.WriteU32(Params + 56u, Records);
    m.WriteU32(Params + 60u, Indices);
    m.WriteU32(Params + 120u, 128u);
    m.WriteU32(Params + 124u, mode == Mode::DirectEmpty ? 0u : 1u);
    m.WriteU32(Params + 140u, 0x3fc00000u);
    m.WriteU32(Params + 220u, Selectors);
    m.WriteU32(Params + 224u, 2u);
    m.WriteU32(Selectors, NodeA);
    m.WriteU32(Selectors + 4u, NodeB);
    m.WriteU32(Selectors + 8u, 0x3f000000u);
    m.WriteU32(Output + 720u, 10u);
    m.WriteU32(NodeA + 60u, 2u);
    m.WriteU32(NodeA + 64u, 5u);
    m.WriteU32(NodeA + 72u, Other);
    m.WriteU32(NodeB + 64u, 15u);
    m.WriteU32(TransformOwner + 72u, Other);
    m.WriteU32(Other + 68u,
        mode == Mode::DirectAboveTransform ||
        mode == Mode::BlendedTransform ? 0x80000000u : 0u);
    m.WriteU16(Indices, 0u);
    m.WriteU32(Records + 92u,
        mode == Mode::DirectFlagged ? 1u : 0u);
    const auto z = mode == Mode::DirectAboveTransform ?
        0x42b40000u : 0x41c80000u;
    m.WriteU32(Records + 16u, 0x3f800000u);
    m.WriteU32(Records + 20u, 0x40000000u);
    m.WriteU32(Records + 24u, z);
    for (unsigned i = 0; i < 64u; ++i)
    {
        m.WriteU32(CurveData + i * 4u, 0x42480000u);
        m.WriteU32(CurveData + 0x100u + i * 4u, 0x42a00000u);
        m.WriteU32(CurveData + 0x200u + i * 4u, 0x41a00000u);
    }
    m.WriteU32(Output + 112u, 0x3f800000u);
    m.WriteU32(Output + 132u, 0x3f800000u);
    m.WriteU32(Output + 152u, 0x3f800000u);
    m.WriteU32(Output + 160u, 0u);
    m.WriteU32(Output + 164u, 0u);
    m.WriteU32(Output + 168u, 0u);
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
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    const auto fpr = std::bit_cast<std::uint64_t>(-7.0);
    c.f0.u64 = fpr; c.f1.u64 = fpr;
    c.f2.u64 = std::bit_cast<std::uint64_t>(0.25);
    c.r7.u64 = 0x2222000000000000ull |
        (Blended(mode) ? Owner : 0u);
    c.f3.u64 = fpr; c.f4.u64 = fpr; c.f5.u64 = fpr;
    c.f6.u64 = fpr; c.f7.u64 = fpr; c.f8.u64 = fpr;
    c.f9.u64 = fpr; c.f10.u64 = fpr; c.f11.u64 = fpr;
    c.f12.u64 = fpr; c.f13.u64 = fpr;
    c.f25.u64 = fpr; c.f26.u64 = fpr; c.f27.u64 = fpr;
    c.f28.u64 = fpr; c.f29.u64 = fpr; c.f30.u64 = fpr;
    c.f31.u64 = fpr;
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
    auto check = [&](const char* label, std::uint64_t p, std::uint64_t q)
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
    Seed(expected.memory, mode); Seed(actual.memory, mode);
    auto context = MakeContext(mode);
    auto state = FromPpc(context);
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    const auto entry = Blended(mode) ? 0x82626e60u : 0x82626c88u;
    if (Blended(mode))
        __imp__sub_82626E60(context, original.Bytes());
    else
        __imp__sub_82626C88(context, original.Bytes());
    active = nullptr;
    const auto expected_events =
        mode == Mode::DirectEmpty || mode == Mode::DirectFlagged ? 0u :
        mode == Mode::DirectVirtual ? 2u : 1u;
    const bool covered = expected.events.size() == expected_events &&
        (mode != Mode::BlendedSelector ||
            expected.memory.ReadU32(Params + 16u) == NodeA);
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(entry, actual.memory, actual,
            actual, actual, state))
        throw std::runtime_error("missing threshold route");
    if (!covered)
        std::fprintf(stderr, "case %u missed box dispatch path\n", ordinal);
    return covered && Compare(context, state, original, recovered,
        expected, actual, ordinal);
}
} // namespace

void OriginalSave25(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u), fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore25(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 25u; i <= 31u; ++i)
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
        std::printf("PASS object-curve-threshold-routes %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 82626C88/82626E60/8242CEB8/82607318/822C7388/822C73E8/822C78D8 bodies; warm singleton, vtable+108/+268 selected-context boundary, finite normal FP; cold construction, guest target internals, faults, MMIO and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
