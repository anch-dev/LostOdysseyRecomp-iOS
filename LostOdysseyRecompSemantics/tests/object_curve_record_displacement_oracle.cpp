#include "lo_semantics/object_curve_record_displacement.h"
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
namespace family = object_curve_record_displacement;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Object = 0x10000u, Params = 0x20000u;
constexpr GuestAddress Indices = 0x30000u, Records = 0x40000u;
constexpr GuestAddress CurveData = 0x50000u, Output = 0x60000u;
constexpr GuestAddress VirtualObject = 0x70000u, Vtable = 0x80000u;
constexpr GuestAddress TransformOwner = 0x90000u, Other = 0xa0000u;
constexpr GuestAddress Vector = 0xb0000u;
constexpr GuestAddress VirtualTarget = 0x82345680u;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x82000000u, 0x4000u}, {0x82189000u, 0x1000u},
    {0x83313000u, 0x1000u}, {0x83315000u, 0x5000u}};
enum class Mode { NormalizeEqual, NormalizeBelow, NormalizeScaled,
    Empty, Flagged, Outside, FallbackHit, VirtualHit,
    ToWorld, ToLocal };
constexpr Mode Cases[] = {Mode::NormalizeEqual, Mode::NormalizeBelow,
    Mode::NormalizeScaled, Mode::Empty, Mode::Flagged, Mode::Outside,
    Mode::FallbackHit, Mode::VirtualHit, Mode::ToWorld, Mode::ToLocal};
bool Standalone(Mode mode)
{ return mode <= Mode::NormalizeScaled; }

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
struct Services final : family::NativeServices
{
    GuestMemory memory;
    std::vector<Event> events;
    explicit Services(test::GuestWindow& window) : memory(window.Memory()) {}
    void SetHostFpControl(std::uint32_t control) override
    { PPCFPSCRRegister{}.setcsr(control); }
    void CallVirtual(GuestAddress target, GuestMemory&,
        family::Registers& s) override
    {
        if (target != VirtualTarget || s.lr != 0x82627b94u)
            throw std::runtime_error("unexpected record virtual call");
        events.push_back({target, Snapshot(s)});
        s.r[3] = 0x1234u;
        s.r[10] = 0xcafef00du;
        s.f1_bits = std::bit_cast<std::uint64_t>(2.0);
        s.f10_bits = std::bit_cast<std::uint64_t>(5.0);
        s.f24_bits = std::bit_cast<std::uint64_t>(0.25);
    }
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
    m.WriteU32(Vector,
        mode == Mode::NormalizeEqual ? 0x3f800000u :
        mode == Mode::NormalizeBelow ? 0x3f000000u : 0x41200000u);
    m.WriteU32(Vector + 4u, 0u); m.WriteU32(Vector + 8u, 0u);
    if (Standalone(mode)) return;
    Curve(m, Object + 96u, CurveData);
    Curve(m, Object + 124u, CurveData + 0x100u);
    if (mode == Mode::VirtualHit)
        m.WriteU32(Object + 120u, VirtualObject);
    m.WriteU32(VirtualObject, Vtable);
    m.WriteU32(Vtable + 268u, VirtualTarget | 3u);
    m.WriteU32(Params + 4u, Params + 4u);
    m.WriteU32(Params + 8u, Output);
    m.WriteU32(Params + 16u, TransformOwner);
    m.WriteU32(TransformOwner + 72u, Other);
    m.WriteU32(Other + 68u, mode == Mode::ToLocal ? 0x80000000u : 0u);
    m.WriteU32(Object + 68u, mode == Mode::ToWorld ? 0x80000000u : 0u);
    m.WriteU32(Params + 56u, Records);
    m.WriteU32(Params + 60u, Indices);
    m.WriteU32(Params + 120u, 128u);
    m.WriteU32(Params + 124u, mode == Mode::Empty ? 0u : 1u);
    m.WriteU16(Indices, 0u);
    m.WriteU32(Records + 92u, mode == Mode::Flagged ? 1u : 0u);
    m.WriteU32(Records + 16u,
        mode == Mode::Outside ? 0x41a00000u : 0x40a00000u);
    m.WriteU32(Records + 20u, 0x3f800000u);
    m.WriteU32(Records + 24u, 0u);
    m.WriteU32(Records + 48u, 0u);
    m.WriteU32(Records + 52u, 0u);
    m.WriteU32(Records + 56u, 0u);
    m.WriteU32(Object + 72u, 0u);
    m.WriteU32(Object + 76u, 0u);
    m.WriteU32(Object + 80u, 0u);
    m.WriteU32(Object + 84u, 0x41200000u);
    m.WriteU32(Object + 88u, 0u);
    m.WriteU32(Object + 92u, 0u);
    // Identity transform for the two representation-conversion branches.
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
    c.r3.u64 = 0xabcdef1200000000ull |
        (Standalone(mode) ? Vector : Object);
    c.r4.u64 = 0x9876543200000000ull | Params;
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.f1.u64 = std::bit_cast<std::uint64_t>(
        Standalone(mode) ? 4.0 : 1.25);
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
    Seed(expected.memory, mode); Seed(actual.memory, mode);
    auto context = MakeContext(mode);
    auto state = FromPpc(context);
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    if (Standalone(mode)) __imp__sub_8229F208(context, original.Bytes());
    else __imp__sub_826276B8(context, original.Bytes());
    active = nullptr;
    const auto expected_events = mode == Mode::VirtualHit ? 1u : 0u;
    bool covered = expected.events.size() == expected_events;
    if (Standalone(mode))
    {
        covered &= context.r3.u32 ==
            (mode == Mode::NormalizeBelow ? 0u : 1u);
        covered &= expected.memory.ReadU32(Vector) ==
            (mode == Mode::NormalizeEqual ? 0x3f800000u :
            mode == Mode::NormalizeBelow ? 0x3f000000u : 0x3f800000u);
    }
    else
    {
        const bool should_write = mode == Mode::FallbackHit ||
            mode == Mode::VirtualHit || mode == Mode::ToWorld ||
            mode == Mode::ToLocal;
        covered &= (expected.memory.ReadU32(Records + 56u) != 0u) ==
            should_write;
    }
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(Standalone(mode) ? 0x8229f208u : 0x826276b8u,
            actual.memory, actual, state))
        throw std::runtime_error("missing record displacement entry");
    if (!covered)
        std::fprintf(stderr, "case %u missed requested record path\n", ordinal);
    return covered && Compare(context, state, original, recovered,
        expected, actual, ordinal);
}
} // namespace

void OriginalSave24(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 24u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u), r[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore24(PPCContext& c, std::uint8_t*)
{
    const auto r = Fields(c);
    for (unsigned i = 24u; i <= 31u; ++i)
        r[i]->u64 = ReadU64(active->memory, Address(c.r1.u64 - 16u -
            (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
void OriginalSaveF14(PPCContext& c, std::uint8_t*)
{
    const auto f = FprFields(c);
    for (unsigned i = 14u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r12.u64 - 144u +
            (i - 14u) * 8u), f[i]->u64);
}
void OriginalRestoreF14(PPCContext& c, std::uint8_t*)
{
    const auto f = FprFields(c);
    for (unsigned i = 14u; i <= 31u; ++i)
        f[i]->u64 = ReadU64(active->memory, Address(c.r12.u64 - 144u +
            (i - 14u) * 8u));
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
        std::printf("PASS object-curve-record-displacement %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT vtable+268 target, exceptional FP, faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
