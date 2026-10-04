#include "lo_semantics/object_sample_accumulator.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_sample_accumulator;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Object = 0x10000u, Selector = 0x20000u;
constexpr GuestAddress Sample0 = 0x30000u, Table = 0x40000u;
constexpr GuestAddress Rows = 0x50000u, PointerArray = 0x60000u;
constexpr GuestAddress Inner = 0x70000u, Output = 0x80000u;
constexpr GuestAddress Total = 0x90000u, SelectorList = 0xa0000u;
constexpr GuestAddress Sample1 = 0xa1000u;
constexpr GuestAddress SelectorWindow = 0xa2000u;
constexpr GuestAddress Samples = 0xb0000u;
constexpr GuestAddress One = 0x82000e50u;
constexpr GuestAddress Floor = 0x82000d90u;
constexpr GuestAddress ModeGlobal = 0x83315ea4u;
constexpr GuestAddress Rng = 0x8331367cu;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x83313000u, 0x1000u},
    {0x83315000u, 0x5000u}};

enum class Mode { LeafSelect, UpperEmpty, UpperOccupied,
    UpperThreshold, UpperFixed, UpperRandom };
constexpr Mode Cases[] = {Mode::LeafSelect, Mode::UpperEmpty,
    Mode::UpperOccupied, Mode::UpperThreshold, Mode::UpperFixed,
    Mode::UpperRandom};

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
    s.f10_bits = c.f10.u64; s.f11_bits = c.f11.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.f30_bits = c.f30.u64; s.f31_bits = c.f31.u64;
    s.cached_fp_control = c.fpscr.csr;
    s.xer_so = c.xer.so; s.xer_ca = c.xer.ca;
    s.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.un};
    return s;
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

void Seed(GuestMemory& m, Mode mode)
{
    m.WriteU32(One, 0x3f800000u);
    m.WriteU32(Floor, 0u);
    m.WriteU32(ModeGlobal, mode == Mode::LeafSelect ||
        mode == Mode::UpperRandom ? 0u : 1u);
    m.WriteU32(Rng, 7u);
    m.WriteU32(Object + 4u, Selector);
    m.WriteU32(Object + 8u, SelectorWindow);
    m.WriteU32(Object + 16u, Sample0);
    m.WriteU32(Object + 184u, PointerArray);
    m.WriteU32(Object + 220u, Samples);
    m.WriteU32(Object + 140u,
        mode == Mode::UpperThreshold ? 0x3e800000u : 0x3f800000u);
    m.WriteU32(Selector + 216u, SelectorList);
    m.WriteU32(Selector + 220u, 2u);
    m.WriteU32(SelectorList, Sample0);
    m.WriteU32(SelectorList + 4u, Sample1);
    m.WriteU32(SelectorWindow + 720u, 10u);
    m.WriteU32(Sample0 + 60u, 0u);
    m.WriteU32(Sample0 + 64u, 5u);
    m.WriteU32(Sample0 + 72u, Table);
    m.WriteU32(Sample1 + 60u, 1u);
    m.WriteU32(Sample1 + 64u, 15u);
    m.WriteU32(Samples, 0x3f400000u);
    m.WriteU32(Samples + 4u, 0x3f800000u);
    m.WriteU32(Table + 120u, Rows);
    m.WriteU32(Table + 124u, mode == Mode::UpperEmpty ? 0u : 1u);
    m.WriteU32(Rows, 8u);
    m.WriteU32(Rows + 4u, mode == Mode::UpperRandom ? 4u : 0u);
    m.WriteU32(Rows + 8u, 0x3f000000u);
    m.WriteU32(PointerArray, Inner);
    m.WriteU32(Inner, mode == Mode::UpperOccupied ? 1u : 0u);
    m.WriteU32(Output, 0x40000000u);
    m.WriteU32(Total, 2u);
}

PPCContext MakeContext(Mode mode)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1234000000000000ull + i * 0x100000001ull;
    c.r1.u64 = 0x1234567800110000ull;
    c.r3.u64 = 0xabcdef1200000000ull |
        (mode == Mode::LeafSelect ? Selector : Object);
    c.r4.u64 = mode == Mode::LeafSelect ? Object : Output;
    c.r5.u64 = Total;
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    c.f1.u64 = std::bit_cast<std::uint64_t>(-1.0);
    c.f10.u64 = std::bit_cast<std::uint64_t>(-10.0);
    c.f11.u64 = std::bit_cast<std::uint64_t>(-11.0);
    c.f12.u64 = std::bit_cast<std::uint64_t>(-12.0);
    c.f13.u64 = std::bit_cast<std::uint64_t>(-13.0);
    c.f30.u64 = std::bit_cast<std::uint64_t>(-30.0);
    c.f31.u64 = std::bit_cast<std::uint64_t>(-31.0);
    c.fpscr.csr = 0x9fc0u;
    c.xer.so = 1; c.xer.ca = 1;
    c.cr6 = {0, 1, 0, {1}};
    return c;
}

bool Compare(PPCContext& expected, const family::Registers& actual,
    const test::GuestWindow& original,
    const test::GuestWindow& recovered, unsigned ordinal)
{
    const auto observed = FromPpc(expected);
    bool same = original.EqualCommitted(recovered);
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
        std::fprintf(stderr, "case %u %s %llx/%llx\n", ordinal,
            label, static_cast<unsigned long long>(x),
            static_cast<unsigned long long>(y));
        same = false;
    };
    check("lr", observed.lr, actual.lr);
    check("ctr", observed.ctr, actual.ctr);
    check("f0", observed.f0_bits, actual.f0_bits);
    check("f1", observed.f1_bits, actual.f1_bits);
    check("f10", observed.f10_bits, actual.f10_bits);
    check("f11", observed.f11_bits, actual.f11_bits);
    check("f12", observed.f12_bits, actual.f12_bits);
    check("f13", observed.f13_bits, actual.f13_bits);
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
    if (!original.EqualCommitted(recovered))
        for (const auto region : Regions)
            for (std::size_t i = 0; i < region.size; ++i)
                if (original.Bytes()[region.base + i] !=
                    recovered.Bytes()[region.base + i])
                {
                    std::fprintf(stderr, "case %u RAM %08llx %02x/%02x\n",
                        ordinal, static_cast<unsigned long long>(region.base + i),
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
    if (mode == Mode::LeafSelect)
        __imp__sub_82607318(context, original.Bytes());
    else
        __imp__sub_822C79B0(context, original.Bytes());
    active = nullptr;
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(mode == Mode::LeafSelect ? 0x82607318u :
            0x822c79b0u, actual.memory, actual, state))
        throw std::runtime_error("missing sample accumulator entry");
    const bool covered = mode != Mode::UpperFixed &&
        mode != Mode::UpperRandom ||
        (expected.memory.ReadU32(Inner) == 1u &&
            expected.memory.ReadU32(Total) != 2u);
    if (!covered)
        std::fprintf(stderr, "case %u missed accumulator write\n", ordinal);
    const bool same = Compare(context, state, original, recovered, ordinal);
    return covered && same;
}
} // namespace

void OriginalSave25(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 25u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore25(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 25u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory,
            Address(c.r1.u64 - 16u - (31u - i) * 8u));
    c.r12.u64 = active->memory.ReadU32(Address(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}

int main()
{
    try
    {
        for (unsigned i = 0; i < std::size(Cases); ++i)
            if (!Check(Cases[i], i)) return 1;
        std::printf("PASS object-sample-accumulator %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 822C79B0/82607318 bodies; finite normal FP and selected PPC state validated; exceptional FP, faults, MMIO and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
