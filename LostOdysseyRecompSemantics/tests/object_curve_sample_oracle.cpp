#include "lo_semantics/object_curve_sample.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
using namespace lo::semantic::gpu;
namespace curve = object_curve_sample;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Object = 0x10000u, Data = 0x20000u;
constexpr GuestAddress Output = 0x30000u;
constexpr GuestAddress VirtualObject = 0x40000u;
constexpr GuestAddress Vtable = 0x50000u;
constexpr GuestAddress VirtualTarget = 0x82345680u;
constexpr GuestAddress BaseOne = 0x82000e50u;
constexpr GuestAddress TypeThreeThreshold = 0x82189798u;
constexpr GuestAddress Rng = 0x8331367cu;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x82189000u, 0x1000u},
    {0x83313000u, 0x1000u}};

enum class Mode { LeafFallback, LeafInterpolate, UpperOne,
    UpperFour, UpperRandom, UpperSelect, WrapperDirect, WrapperVirtual };
constexpr Mode Cases[] = {Mode::LeafFallback, Mode::LeafInterpolate,
    Mode::UpperOne, Mode::UpperFour, Mode::UpperRandom,
    Mode::UpperSelect, Mode::WrapperDirect, Mode::WrapperVirtual};

std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
curve::Registers FromPpc(PPCContext& c)
{
    curve::Registers s{};
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
void ToPpc(PPCContext& c, const curve::Registers& s)
{
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i) fields[i]->u64 = s.r[i];
    c.lr = s.lr; c.ctr.u64 = s.ctr;
    c.f0.u64 = s.f0_bits; c.f1.u64 = s.f1_bits;
    c.f10.u64 = s.f10_bits; c.f11.u64 = s.f11_bits;
    c.f12.u64 = s.f12_bits; c.f13.u64 = s.f13_bits;
    c.f30.u64 = s.f30_bits; c.f31.u64 = s.f31_bits;
    c.fpscr.csr = s.cached_fp_control;
    c.xer.so = s.xer_so; c.xer.ca = s.xer_ca;
    c.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, {s.cr6.un}};
}

struct Event
{
    GuestAddress target;
    std::uint64_t r1, r3, lr, ctr;
    bool operator==(const Event&) const = default;
};
struct Services final : curve::NativeServices
{
    GuestMemory memory;
    std::vector<Event> events;
    explicit Services(test::GuestWindow& window) : memory(window.Memory()) {}
    void SetHostFpControl(std::uint32_t control) override
    { PPCFPSCRRegister{}.setcsr(control); }
    void CallVirtual(GuestAddress target, GuestMemory& guest,
        curve::Registers& state) override
    {
        if (target != VirtualTarget)
            throw std::runtime_error("unexpected curve virtual target");
        events.push_back({target, state.r[1], state.r[3],
            state.lr, state.ctr});
        state.r[3] = 0x1234u;
        state.f1_bits = std::bit_cast<std::uint64_t>(7.5);
        guest.WriteU32(Output, 0x44556677u);
    }
};
Services* active = nullptr;

struct RestoreHost
{
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};

void Initialize(GuestMemory& m, Mode mode)
{
    m.WriteU32(BaseOne, 0x3f800000u);
    m.WriteU32(TypeThreeThreshold, 0x3e800000u);
    m.WriteU32(Rng, 7u);
    m.WriteU8(Object + 1u, mode == Mode::UpperRandom ? 2u :
        mode == Mode::UpperSelect ? 3u : 1u);
    m.WriteU8(Object + 2u, 1u);
    m.WriteU8(Object + 3u, 1u);
    m.WriteU32(Object + 4u, Data);
    m.WriteU32(Object + 8u, 20u);
    m.WriteU32(Object + 16u, 0x3f800000u);
    m.WriteU32(Object + 20u, 0u);
    m.WriteU32(Object + 24u,
        mode == Mode::WrapperVirtual ? VirtualObject : 0u);
    m.WriteU32(VirtualObject, Vtable);
    m.WriteU32(Vtable + 268u, VirtualTarget | 3u);
    for (unsigned i = 0; i < 64u; ++i)
        m.WriteU32(Data + i * 4u,
            std::bit_cast<std::uint32_t>(float(i) * 0.25f));
}

GuestAddress Entry(Mode mode)
{
    if (mode == Mode::LeafFallback || mode == Mode::LeafInterpolate)
        return 0x822c78d8u;
    if (mode == Mode::WrapperDirect || mode == Mode::WrapperVirtual)
        return 0x822c7388u;
    return 0x822c73e8u;
}
PPCContext MakeContext(Mode mode)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1234000000000000ull + i * 0x100000001ull;
    c.r1.u64 = 0x1234567800110000ull;
    c.r3.u64 = 0xabcdef1200000000ull | Object;
    c.r5.u64 = mode == Mode::LeafFallback ||
        mode == Mode::LeafInterpolate ? Stack - 108u : Output;
    c.r6.u64 = mode == Mode::LeafFallback ||
        mode == Mode::LeafInterpolate ? Stack - 104u :
        mode == Mode::UpperOne ? 1u :
        mode == Mode::UpperFour || mode == Mode::UpperRandom ||
        mode == Mode::UpperSelect ? 4u : 0x6666u;
    c.r7.u64 = Stack - 100u;
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    c.f1.u64 = std::bit_cast<std::uint64_t>(
        mode == Mode::LeafFallback || mode == Mode::UpperOne ? -0.5 : 1.5);
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

bool Compare(PPCContext& expected, const curve::Registers& actual,
    const test::GuestWindow& original,
    const test::GuestWindow& recovered,
    const Services& a, const Services& b, unsigned ordinal)
{
    const auto converted = FromPpc(expected);
    bool same = original.EqualCommitted(recovered) && a.events == b.events;
    for (unsigned i = 0; i < 32u; ++i)
        if (converted.r[i] != actual.r[i])
        {
            std::fprintf(stderr, "case %u r%u %llx/%llx\n", ordinal, i,
                static_cast<unsigned long long>(converted.r[i]),
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
    check("lr", converted.lr, actual.lr);
    check("ctr", converted.ctr, actual.ctr);
    check("f0", converted.f0_bits, actual.f0_bits);
    check("f1", converted.f1_bits, actual.f1_bits);
    check("f10", converted.f10_bits, actual.f10_bits);
    check("f11", converted.f11_bits, actual.f11_bits);
    check("f12", converted.f12_bits, actual.f12_bits);
    check("f13", converted.f13_bits, actual.f13_bits);
    check("f30", converted.f30_bits, actual.f30_bits);
    check("f31", converted.f31_bits, actual.f31_bits);
    check("fp-control", converted.cached_fp_control,
        actual.cached_fp_control);
    check("xer-so", converted.xer_so, actual.xer_so);
    check("xer-ca", converted.xer_ca, actual.xer_ca);
    check("cr-lt", converted.cr6.lt, actual.cr6.lt);
    check("cr-gt", converted.cr6.gt, actual.cr6.gt);
    check("cr-eq", converted.cr6.eq, actual.cr6.eq);
    check("cr-un", converted.cr6.un, actual.cr6.un);
    if (a.events != b.events)
        std::fprintf(stderr, "case %u virtual events %zu/%zu\n",
            ordinal, a.events.size(), b.events.size());
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
    Initialize(expected.memory, mode);
    Initialize(actual.memory, mode);
    auto context = MakeContext(mode);
    auto state = FromPpc(context);
    PPCFPSCRRegister{}.setcsr(context.fpscr.csr);
    active = &expected;
    switch (Entry(mode))
    {
    case 0x822c7388u: __imp__sub_822C7388(context, original.Bytes()); break;
    case 0x822c73e8u: __imp__sub_822C73E8(context, original.Bytes()); break;
    case 0x822c78d8u: __imp__sub_822C78D8(context, original.Bytes()); break;
    }
    active = nullptr;
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!curve::Apply(Entry(mode), actual.memory, actual, state))
        throw std::runtime_error("missing curve entry");
    const bool covered = expected.events.size() ==
            (mode == Mode::WrapperVirtual ? 1u : 0u) &&
        (mode != Mode::UpperRandom && mode != Mode::UpperSelect ||
            expected.memory.ReadU32(Rng) != 7u);
    if (!covered)
        std::fprintf(stderr, "case %u did not traverse its target branch\n",
            ordinal);
    const bool compared = Compare(context, state, original, recovered,
        expected, actual, ordinal);
    return covered && compared;
}
} // namespace

void OriginalSave27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore27(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 27u; i <= 31u; ++i)
        fields[i]->u64 = ReadU64(active->memory,
            Address(c.r1.u64 - 16u - (31u - i) * 8u));
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
        std::printf("PASS object-curve-sample %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 822C7388/822C73E8/822C78D8 bodies with finite normal FP; vtable+268 target remains mutable; faults, MMIO, exceptional FP, generic ABI and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
