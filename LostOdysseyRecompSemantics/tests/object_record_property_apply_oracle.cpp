#include "lo_semantics/object_record_property_apply.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = object_record_property_apply;
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
constexpr GuestAddress Stack = 0x110000u;
constexpr GuestAddress Object = 0x10000u, Data = 0x20000u;
constexpr GuestAddress Records = 0x30000u, Indices = 0x40000u;
constexpr GuestAddress Elements = 0x50000u, Output = 0x60000u;
constexpr GuestAddress KeyBase = 0x832c0000u;
constexpr std::uint64_t Key2 = 0x1111222233334444ull;
constexpr std::uint64_t Key1 = 0x5555666677778888ull;
constexpr std::uint64_t Key3 = 0x9999aaaabbbbccccull;
constexpr test::Region Regions[] = {{0u, 0x120000u},
    {0x832c1000u, 0x1000u}};

enum class Mode { LeafVector, LeafScalar, NoMatch,
    VectorOnly, Flagged, AllTwo };
constexpr Mode Cases[] = {Mode::LeafVector, Mode::LeafScalar,
    Mode::NoMatch, Mode::VectorOnly, Mode::Flagged, Mode::AllTwo};

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
    s.f7_bits = c.f7.u64; s.f8_bits = c.f8.u64;
    s.f9_bits = c.f9.u64; s.f10_bits = c.f10.u64;
    s.f11_bits = c.f11.u64; s.f12_bits = c.f12.u64;
    s.f13_bits = c.f13.u64; s.f30_bits = c.f30.u64;
    s.f31_bits = c.f31.u64;
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
void Record(GuestMemory& m, GuestAddress address, std::uint64_t key,
    unsigned type, std::uint32_t scalar,
    std::array<std::uint32_t, 3> vector)
{
    WriteU64(m, address, key);
    m.WriteU8(address + 8u, static_cast<std::uint8_t>(type));
    m.WriteU32(address + 12u, scalar);
    for (unsigned i = 0; i < 3u; ++i)
        m.WriteU32(address + 16u + i * 4u, vector[i]);
}
void Seed(GuestMemory& m, Mode mode)
{
    WriteU64(m, KeyBase + 6248u, Key2);
    WriteU64(m, KeyBase + 6256u, Key1);
    WriteU64(m, KeyBase + 6264u, Key3);
    m.WriteU32(Object + 8u, Data);
    m.WriteU32(Object + 56u, Elements);
    m.WriteU32(Object + 60u, Indices);
    m.WriteU32(Object + 120u, 128u);
    m.WriteU32(Object + 124u, 2u);
    m.WriteU16(Indices, 0u);
    m.WriteU16(Indices + 2u, 1u);
    m.WriteU32(Data + 672u, Records);
    m.WriteU32(Data + 676u, mode == Mode::NoMatch ? 0u :
        mode == Mode::VectorOnly || mode == Mode::Flagged ? 1u : 3u);
    m.WriteU32(Data + 732u, mode == Mode::Flagged ? 0x200000u : 0u);
    m.WriteU32(Data + 892u, 0x3e800000u);
    Record(m, Records, Key2, 2u, 0u,
        {0x40000000u, 0x40400000u, 0x40800000u});
    Record(m, Records + 44u, Key1, 1u, 0x3f000000u, {0u, 0u, 0u});
    Record(m, Records + 88u, Key3, 1u, 0x3f400000u, {0u, 0u, 0u});
    for (unsigned i = 0; i < 2u; ++i)
    {
        const auto e = Elements + i * 128u;
        for (unsigned j = 0; j < 3u; ++j)
        {
            m.WriteU32(e + 32u + j * 4u,
                std::bit_cast<std::uint32_t>(float(j + 1u)));
            m.WriteU32(e + 96u + j * 4u,
                std::bit_cast<std::uint32_t>(float(j + 2u)));
        }
        m.WriteU32(e + 108u, 0x40000000u);
    }
    for (unsigned i = 0; i < 5u; ++i)
        m.WriteU32(Stack - 64u + i * 4u, 0x3f800000u);
}
PPCContext MakeContext(Mode mode)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1234000000000000ull + i * 0x100000001ull;
    c.r1.u64 = 0x1234567800110000ull;
    c.r3.u64 = 0xabcdef1200000000ull |
        (mode == Mode::LeafVector || mode == Mode::LeafScalar ? Data : Object);
    c.r4.u64 = mode == Mode::LeafVector ? Key2 :
        mode == Mode::LeafScalar ? Key1 : 0x11112222u;
    c.r5.u64 = Output;
    c.lr = 0x1111222233334444ull;
    c.ctr.u64 = 0x5555666677778888ull;
    c.f0.u64 = std::bit_cast<std::uint64_t>(-7.0);
    c.f1.u64 = std::bit_cast<std::uint64_t>(-1.0);
    c.f7.u64 = std::bit_cast<std::uint64_t>(-7.5);
    c.f8.u64 = std::bit_cast<std::uint64_t>(-8.5);
    c.f9.u64 = std::bit_cast<std::uint64_t>(-9.5);
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
    check("f7", observed.f7_bits, actual.f7_bits);
    check("f8", observed.f8_bits, actual.f8_bits);
    check("f9", observed.f9_bits, actual.f9_bits);
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
    const GuestAddress entry = mode == Mode::LeafVector ? 0x822c85b0u :
        mode == Mode::LeafScalar ? 0x822c7b88u : 0x822c8430u;
    if (entry == 0x822c85b0u)
        __imp__sub_822C85B0(context, original.Bytes());
    else if (entry == 0x822c7b88u)
        __imp__sub_822C7B88(context, original.Bytes());
    else
        __imp__sub_822C8430(context, original.Bytes());
    active = nullptr;
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (!family::Apply(entry, actual.memory, actual, state))
        throw std::runtime_error("missing record property entry");
    const bool covered = mode != Mode::AllTwo ||
        expected.memory.ReadU32(Elements + 96u) != 0x40000000u;
    if (!covered)
        std::fprintf(stderr, "case %u missed property update\n", ordinal);
    const bool same = Compare(context, state, original, recovered, ordinal);
    return covered && same;
}
} // namespace

void OriginalSave28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
        WriteU64(active->memory, Address(c.r1.u64 - 16u - (31u - i) * 8u),
            fields[i]->u64);
    active->memory.WriteU32(Address(c.r1.u64 - 8u), c.r12.u32);
}
void OriginalRestore28(PPCContext& c, std::uint8_t*)
{
    const auto fields = Fields(c);
    for (unsigned i = 28u; i <= 31u; ++i)
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
        std::printf("PASS object-record-property-apply %zu actual PPC cases\n",
            std::size(Cases));
        std::puts("LIMIT actual 822C8430/822C85B0/822C7B88 bodies with finite normal FP and selected PPC state; exceptional FP, faults, MMIO and runtime excluded");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
