#include "lo_semantics/object_curve_control_point_blend.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_curve_control_point_blend;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u, Object = 0x10000u;
constexpr GuestAddress Records = 0x20000u, Weight = 0x50000u;
constexpr GuestAddress Output = 0x60000u;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x82000000u, 0x4000u}, {0x82189000u, 0x1000u}};
enum class Mode { Empty, Single, FirstOne, LastOne,
    MiddleBlend, MiddleZero, MiddleSkip, Standalone };
constexpr Mode Cases[] = {Mode::Empty, Mode::Single, Mode::FirstOne,
    Mode::LastOne, Mode::MiddleBlend, Mode::MiddleZero,
    Mode::MiddleSkip, Mode::Standalone};

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
        &s.f4_bits, &s.f5_bits, &s.f6_bits, &s.f7_bits,
        &s.f8_bits, &s.f9_bits, &s.f10_bits, &s.f11_bits, &s.f12_bits,
        &s.f13_bits, &s.f14_bits, &s.f15_bits, &s.f16_bits, &s.f17_bits,
        &s.f18_bits, &s.f19_bits, &s.f20_bits, &s.f21_bits, &s.f22_bits,
        &s.f23_bits, &s.f24_bits, &s.f25_bits, &s.f26_bits, &s.f27_bits,
        &s.f28_bits, &s.f29_bits, &s.f30_bits, &s.f31_bits};
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
    s.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.un};
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return s;
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
struct Services final : family::NativeServices
{
    GuestMemory memory;
    explicit Services(test::GuestWindow& window) : memory(window.Memory()) {}
    void SetHostFpControl(std::uint32_t control) override
    { PPCFPSCRRegister{}.setcsr(control); }
};
Services* active = nullptr;
struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
std::uint32_t Float(float value)
{ return std::bit_cast<std::uint32_t>(value); }
unsigned Count(Mode mode)
{
    if (mode == Mode::Empty) return 0u;
    if (mode == Mode::Single) return 1u;
    if (mode == Mode::FirstOne || mode == Mode::LastOne) return 2u;
    return 3u;
}
std::uint8_t Type(Mode mode, unsigned i)
{
    if (mode == Mode::FirstOne && i == 0u) return 1u;
    if (mode == Mode::LastOne && i == 1u) return 1u;
    if (mode == Mode::MiddleBlend && i < 2u) return 1u;
    if (mode == Mode::MiddleZero && i == 1u) return 1u;
    if (mode == Mode::MiddleSkip && i == 1u) return 0u;
    return 2u;
}
void Seed(GuestMemory& m, Mode mode)
{
    m.WriteU32(0x82000e50u, Float(0.25f));
    m.WriteU32(0x8218958cu, Float(1.0f));
    m.WriteU32(0x82189798u, Float(0.5f));
    m.WriteU32(Weight, Float(0.25f));
    m.WriteU32(Object, Records);
    m.WriteU32(Object + 4u, Count(mode));
    for (unsigned i = 0; i < 3u; ++i)
    {
        const auto record = Records + i * 80u;
        for (unsigned j = 0; j < 6u; ++j)
        {
            m.WriteU32(record + 4u + j * 4u,
                Float(float(1u + i * 3u + j)));
            m.WriteU32(record + 28u + j * 4u,
                Float(float(2u + i * 2u + j)));
            m.WriteU32(record + 52u + j * 4u,
                Float(float(4u + i * 2u + j)));
        }
        m.WriteU8(record + 76u, Type(mode, i));
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
    if (mode == Mode::Standalone)
    {
        c.r3.u64 = 0xabcdef1200000000ull | (Records + 4u);
        c.r4.u64 = 0x9876543200000000ull | (Records + 84u);
        c.r5.u64 = 0x5555000000000000ull | (Records + 164u);
        c.r6.u64 = 0x7777000000000000ull | Weight;
        c.r7.u64 = 0x8888000000000000ull | Output;
    }
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.f1.u64 = std::bit_cast<std::uint64_t>(1.5);
    c.fpscr.csr = 0x9fc0u;
    c.xer.so = 1; c.xer.ca = 1;
    c.cr0 = {0, 1, 0, {1}};
    c.cr6 = {1, 0, 0, {1}};
    return c;
}
bool Compare(PPCContext& expected, const family::Registers& actual,
    const test::GuestWindow& original, const test::GuestWindow& recovered,
    unsigned ordinal)
{
    const auto observed = FromPpc(expected);
    const auto x = Snapshot(observed), y = Snapshot(actual);
    bool same = original.EqualCommitted(recovered);
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
    check("cr0-lt", observed.cr0.lt, actual.cr0.lt);
    check("cr0-gt", observed.cr0.gt, actual.cr0.gt);
    check("cr0-eq", observed.cr0.eq, actual.cr0.eq);
    check("cr0-un", observed.cr0.un, actual.cr0.un);
    check("cr6-lt", observed.cr6.lt, actual.cr6.lt);
    check("cr6-gt", observed.cr6.gt, actual.cr6.gt);
    check("cr6-eq", observed.cr6.eq, actual.cr6.eq);
    check("cr6-un", observed.cr6.un, actual.cr6.un);
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
bool ZeroWords(GuestMemory& m, GuestAddress address)
{
    for (unsigned i = 0; i < 6u; ++i)
        if (m.ReadU32(address + i * 4u) != 0u) return false;
    return true;
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
    if (mode == Mode::Standalone)
        __imp__sub_8262C4B0(context, original.Bytes());
    else __imp__sub_8262B610(context, original.Bytes());
    active = nullptr;
    bool covered = true;
    if (mode == Mode::Single || mode == Mode::FirstOne)
        covered &= ZeroWords(expected.memory, Records + 52u);
    if (mode == Mode::LastOne)
        covered &= ZeroWords(expected.memory, Records + 80u + 28u);
    if (mode == Mode::MiddleZero)
        covered &= ZeroWords(expected.memory, Records + 80u + 28u) &&
            ZeroWords(expected.memory, Records + 80u + 52u);
    if (mode == Mode::MiddleBlend)
        covered &= expected.memory.ReadU32(Records + 80u + 52u) !=
            Float(6.0f);
    if (mode == Mode::MiddleSkip)
        covered &= expected.memory.ReadU32(Records + 80u + 52u) ==
            Float(6.0f);
    if (mode == Mode::Standalone)
        covered &= expected.memory.ReadU32(Output) != 0xbdbdbdbdu;
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(mode == Mode::Standalone ? 0x8262c4b0u :
        0x8262b610u, actual.memory, actual, state))
        throw std::runtime_error("missing control point blend entry");
    if (!covered)
        std::fprintf(stderr, "case %u missed requested control path\n", ordinal);
    return covered && Compare(context, state, original, recovered, ordinal);
}
} // namespace

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
int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-curve-control-point-blend %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT finite normal FP and valid record spans only; faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
