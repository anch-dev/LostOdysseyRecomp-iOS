#include "lo_semantics/legacy_descriptor_match_caller.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_match_caller;
constexpr GuestAddress Stack = 0x80000u, Object = 0x10080u;
constexpr GuestAddress Descriptor = 0x20000u, Terminal = 0x22000u;
constexpr GuestAddress Child = 0x30000u, Other = 0x31000u;
constexpr GuestAddress Record = 0x32000u, NumericDescriptor = 0x33000u;
constexpr GuestAddress Header = 0x40000u, Owner = 0x50000u;
constexpr GuestAddress Pool = 0x60000u, Output = 0x70000u;
constexpr GuestAddress SelectorTable = 0x821712a0u;
constexpr std::array<test::Region, 4> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x10000u},
    {0x8210a000u, 0x2000u}, {0x82171000u, 0x1000u}
}};
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};
enum class Path { Guard, Unknown, Type111, Type109, Reject109,
    Type3Match, Type3Mismatch, Type13Numeric, Type102Then124 };
struct Case { const char* name; Path path; };
constexpr std::array Cases{
    Case{"guard-bit-exit", Path::Guard},
    Case{"type111-context-exit", Path::Type111},
    Case{"type109-follow-child", Path::Type109},
    Case{"type109-reject-flags", Path::Reject109},
    Case{"type3-equal-descriptors", Path::Type3Match},
    Case{"type3-different-descriptors", Path::Type3Mismatch},
    Case{"type13-nonzero-numeric", Path::Type13Numeric},
    Case{"type102-then124-typed-scalar", Path::Type102Then124}
};
unsigned selector_calls = 0, numeric_calls = 0, match_calls = 0;
unsigned typed_calls = 0, lookup_calls = 0, constructor_calls = 0;

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
    auto& g = s.numeric.classifier.integer;
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) g.r[i] = fields[i]->u64;
    g.sp = c.r1.u64; g.lr = c.lr; g.ctr = c.ctr.u64;
    g.xer_so = c.xer.so; g.xer_ca = c.xer.ca;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    s.numeric.classifier.f0_bits = c.f0.u64;
    s.numeric.classifier.f1_bits = c.f1.u64;
    s.numeric.classifier.cached_fp_control = c.fpscr.csr;
    s.numeric.f31_bits = c.f31.u64;
    s.f2_bits = c.f2.u64; s.f3_bits = c.f3.u64; s.f4_bits = c.f4.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    return s;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }
bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.numeric.classifier.integer;
    const auto& y = b.numeric.classifier.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.numeric.classifier.f0_bits == b.numeric.classifier.f0_bits &&
        a.numeric.classifier.f1_bits == b.numeric.classifier.f1_bits &&
        a.numeric.classifier.cached_fp_control == b.numeric.classifier.cached_fp_control &&
        a.numeric.f31_bits == b.numeric.f31_bits &&
        a.f2_bits == b.f2_bits && a.f3_bits == b.f3_bits &&
        a.f4_bits == b.f4_bits && a.f12_bits == b.f12_bits &&
        a.f13_bits == b.f13_bits;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{}; const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1122334400000000ull + i;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = 0xabcdef0100000000ull | Object;
    c.r4.u64 = 0; c.r5.u64 = Output; c.r6.u64 = Output + 4u;
    c.r7.u64 = Output + 8u;
    c.r8.u64 = item.path == Path::Type111 ? 1u : 0u;
    c.r9.u64 = 0; c.r10.u64 = item.path == Path::Guard ? 1u : 0u;
    c.f0.u64 = std::bit_cast<std::uint64_t>(9.0);
    c.f1.u64 = std::bit_cast<std::uint64_t>(3.5);
    c.f2.u64 = std::bit_cast<std::uint64_t>(1.0);
    c.f3.u64 = std::bit_cast<std::uint64_t>(-2.0);
    c.f4.u64 = std::bit_cast<std::uint64_t>(4.0);
    c.f12.u64 = std::bit_cast<std::uint64_t>(7.0);
    c.f13.u64 = std::bit_cast<std::uint64_t>(8.0);
    c.f31.u64 = std::bit_cast<std::uint64_t>(-5.0);
    c.lr = 0xabcdef0123456789ull; c.ctr.u64 = 0x5555666677778888ull;
    c.cr0.gt = 1; c.cr6.lt = 1; c.xer.so = 1; c.xer.ca = 1;
    c.fpscr.csr = 0x9fc0u;
    return c;
}
void Seed(GuestMemory& m, const Case& item)
{
    // Image bytes at VA 821712A0 (offset 001712A0) and constant 82000FE8.
    for (unsigned i = 0; i < SelectorBytes.size(); ++i)
        m.WriteU8(SelectorTable + i, SelectorBytes[i]);
    recovery_abi::WriteU64(m, 0x82000fe8u, 0u);
    m.WriteU32(0x82000e50u, 0u); m.WriteU32(0x82007784u, 0x3f800000u);
    recovery_abi::WriteU64(m, 0x82000f28u, 0x3ff0000000000000ull);
    recovery_abi::WriteU64(m, 0x8210aac8u, 0x41f0000000000000ull);
    m.WriteU32(0x10000u, Header); m.WriteU32(Header + 148u, Owner);
    m.WriteU32(Object, item.path == Path::Guard ? 1u : 0u);
    m.WriteU32(Object + 12u, Descriptor);
    m.WriteU32(Terminal + 8u, 1u << 7u);
    m.WriteU32(Child, item.path == Path::Reject109 ? 0x10u : 0u);
    m.WriteU32(Child + 12u, Terminal);
    m.WriteU32(Other, item.path == Path::Type3Mismatch ? 1u : 0u);
    m.WriteU32(Other + 12u, Terminal);
    unsigned kind = 1u;
    switch (item.path)
    {
    case Path::Guard: kind = 109u; break;
    case Path::Unknown: kind = 42u; break;
    case Path::Type111: kind = 111u; break;
    case Path::Type109: case Path::Reject109: kind = 109u; break;
    case Path::Type3Match: case Path::Type3Mismatch: kind = 3u; break;
    case Path::Type13Numeric: kind = 13u; break;
    case Path::Type102Then124: kind = 102u; break;
    }
    m.WriteU32(Descriptor + 8u, kind << 7u);
    if (kind == 109u) m.WriteU32(Descriptor + 40u, Child);
    if (kind == 3u)
    {
        m.WriteU32(Descriptor + 40u, Child);
        m.WriteU32(Descriptor + 44u, Other);
    }
    if (kind == 13u)
    {
        m.WriteU32(Descriptor + 40u, Record);
        m.WriteU32(Descriptor + 44u, Child);
        m.WriteU32(Descriptor + 48u, Other);
        m.WriteU32(Record, Header);
        m.WriteU32(Record + 12u, NumericDescriptor);
        m.WriteU32(NumericDescriptor + 8u, 124u << 7u);
        m.WriteU32(Record + 40u, 0x40600000u);
    }
    if (kind == 102u)
    {
        constexpr GuestAddress Next = 0x21000u;
        m.WriteU32(Descriptor + 40u, 1u);
        m.WriteU32(1u, 0u); m.WriteU32(13u, Next);
        m.WriteU32(Next, Header); m.WriteU32(Next + 8u, 124u << 7u);
        m.WriteU32(Next + 16u, 0x4000u);
        m.WriteU32(Next + 40u, 5u);
    }
    m.WriteU32(Owner + 4u, Owner); m.WriteU32(Owner + 16u, Child);
    m.WriteU32(16u, Child); m.WriteU32(Owner + 40u, 0u);
    m.WriteU32(Owner + 88u, 0u);
    m.WriteU32(Owner + 912u, Pool + 4096u);
    m.WriteU32(Owner + 916u, Pool);
    for (unsigned slot = 15u; slot < 22u; ++slot)
        m.WriteU32(Owner + slot * 4u, 0u);
    m.WriteU32(Output, 0xaaaaaaaa);
    m.WriteU32(Output + 4u, 0xbbbbbbbb);
    m.WriteU32(Output + 8u, 0xcccccccc);
}

struct Services final : family::DiagnosticServices,
    legacy_descriptor_numeric_match_routes::Services,
    legacy_descriptor_scalar_intern_callers::DiagnosticServices,
    legacy_descriptor_array_allocation::Services,
    legacy_descriptor_array_lookup::FreeTailServices
{
    void SetHostFpControl(std::uint32_t csr) override { simde_mm_setcsr(csr); }
    void Diagnostic(GuestMemory&, legacy_descriptor_numeric_match_routes::Registers&) override
    { std::abort(); }
    void Call(GuestAddress, GuestMemory&, family::Registers&) override
    { std::abort(); }
    void Call(GuestMemory&, legacy_descriptor_scalar_intern_callers::Registers&) override
    { std::abort(); }
    void AllocateFromPool(GuestMemory&, crt_stream_operations::Registers&) override
    { std::abort(); }
    void Call(GuestAddress, GuestMemory&, crt_stream_operations::Registers&) override
    { std::abort(); }
};

void Check(const Case& item)
{
    test::GuestWindow original(Regions), recovered(Regions);
    auto left = original.Memory(), right = recovered.Memory();
    Seed(left, item); Seed(right, item);
    auto context = Initial(item); auto state = FromPpc(context);
    Services services;
    selector_calls = numeric_calls = match_calls = typed_calls = 0;
    lookup_calls = constructor_calls = 0;
    const auto saved = simde_mm_getcsr(); simde_mm_setcsr(0x9fc0u);
    std::fprintf(stderr, "match-caller %s original\n", item.name);
    std::fflush(stderr);
    __imp__sub_8305A868(context, original.Bytes());
    const auto observed = FromPpc(context);
    const auto original_csr = simde_mm_getcsr(); simde_mm_setcsr(0x9fc0u);
    std::fprintf(stderr, "match-caller %s recovered\n", item.name);
    std::fflush(stderr);
    if (!family::Apply(0x8305a868u, right,
            {services, {{services, services}, services}, services}, state))
        throw std::runtime_error("caller implementation missing");
    const auto recovered_csr = simde_mm_getcsr(); simde_mm_setcsr(saved);
    if (item.path == Path::Type109 &&
        (left.ReadU32(Output) != Terminal || left.ReadU32(Output + 4u) != 0u ||
            left.ReadU32(Output + 8u) != 0u))
        throw std::runtime_error("type109 child result fields");
    if (item.path == Path::Type3Match && match_calls != 1u)
        throw std::runtime_error("type3 actual predicate not called");
    if (item.path == Path::Type13Numeric && numeric_calls != 1u)
        throw std::runtime_error("type13 actual numeric lower not called");
    if (item.path == Path::Type102Then124 &&
        // One selector call belongs to this caller; the second belongs to
        // the actual descriptor constructor reached through typed intern.
        (selector_calls != 2u || typed_calls != 1u || lookup_calls != 1u ||
            constructor_calls != 1u))
    {
        std::fprintf(stderr,
            "type102/124 calls selector=%u typed=%u lookup=%u constructor=%u; output=%08X/%08X/%08X\n",
            selector_calls, typed_calls, lookup_calls, constructor_calls,
            left.ReadU32(Output), left.ReadU32(Output + 4u),
            left.ReadU32(Output + 8u));
        throw std::runtime_error("type102/124 actual chain not called");
    }
    if (!Same(observed, state) || original_csr != recovered_csr ||
        !original.EqualCommitted(recovered))
    {
        const auto& x = observed.numeric.classifier.integer;
        const auto& y = state.numeric.classifier.integer;
        for (unsigned i = 0; i < 32u; ++i)
            if (x.r[i] != y.r[i])
                std::fprintf(stderr, "r%u %016llX/%016llX\n", i,
                    (unsigned long long)x.r[i], (unsigned long long)y.r[i]);
        std::fprintf(stderr, "%s SP %llX/%llX LR %llX/%llX F0 %llX/%llX F1 %llX/%llX F31 %llX/%llX CSR %08X/%08X\n",
            item.name, (unsigned long long)x.sp, (unsigned long long)y.sp,
            (unsigned long long)x.lr, (unsigned long long)y.lr,
            (unsigned long long)observed.numeric.classifier.f0_bits,
            (unsigned long long)state.numeric.classifier.f0_bits,
            (unsigned long long)observed.numeric.classifier.f1_bits,
            (unsigned long long)state.numeric.classifier.f1_bits,
            (unsigned long long)observed.numeric.f31_bits,
            (unsigned long long)state.numeric.f31_bits, original_csr, recovered_csr);
        for (auto region : Regions)
            for (std::size_t off = 0; off < region.size; ++off)
            {
                const auto a = std::size_t(region.base) + off;
                if (original.Bytes()[a] != recovered.Bytes()[a])
                {
                    std::fprintf(stderr, "RAM %08zX %02X/%02X\n", a,
                        original.Bytes()[a], recovered.Bytes()[a]);
                    throw std::runtime_error(item.name);
                }
            }
        throw std::runtime_error(item.name);
    }
}
} // namespace

void OriginalBody82FAC238(PPCContext& c, std::uint8_t* b)
{ ++selector_calls; __imp__sub_82FAC238(c, b); }
void OriginalBody83053EA0(PPCContext& c, std::uint8_t* b)
{ ++numeric_calls; __imp__sub_83053EA0(c, b); }
void OriginalBody82FAC128(PPCContext& c, std::uint8_t* b)
{ ++match_calls; __imp__sub_82FAC128(c, b); }
void OriginalBody8305A7C0(PPCContext& c, std::uint8_t* b)
{ ++typed_calls; __imp__sub_8305A7C0(c, b); }
void OriginalBody83058AD8(PPCContext& c, std::uint8_t* b)
{ ++lookup_calls; __imp__sub_83058AD8(c, b); }
void OriginalBody83058590(PPCContext& c, std::uint8_t* b)
{ ++constructor_calls; __imp__sub_83058590(c, b); }
void OriginalNative82F99D98(PPCContext&, std::uint8_t*) { std::abort(); }
void OriginalNative82FAC428(PPCContext&, std::uint8_t*) { std::abort(); }
void OriginalNative827C9C60(PPCContext&, std::uint8_t*) { std::abort(); }
void OriginalNative823ADE28(PPCContext&, std::uint8_t*) { std::abort(); }
void OriginalNativeMmFreePhysicalMemory(PPCContext&, std::uint8_t*)
{ std::abort(); }

namespace
{
void Save(unsigned first, PPCContext& c, std::uint8_t* b)
{
    auto m = GuestMemory(0u, std::span<std::uint8_t>(b, test::GuestWindow::Space));
    const auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        recovery_abi::WriteU64(m, c.r1.u32 - 8u * (33u - i), fields[i]->u64);
    m.WriteU32(c.r1.u32 - 8u, c.r12.u32);
}
void Restore(unsigned first, PPCContext& c, std::uint8_t* b)
{
    auto m = GuestMemory(0u, std::span<std::uint8_t>(b, test::GuestWindow::Space));
    const auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        fields[i]->u64 = recovery_abi::ReadU64(m, c.r1.u32 - 8u * (33u - i));
    c.r12.u64 = m.ReadU32(c.r1.u32 - 8u);
    c.lr = c.r12.u64;
}
}
void OriginalSave17(PPCContext& c, std::uint8_t* b) { Save(17u, c, b); }
void OriginalRestore17(PPCContext& c, std::uint8_t* b) { Restore(17u, c, b); }
void OriginalSave24(PPCContext& c, std::uint8_t* b) { Save(24u, c, b); }
void OriginalRestore24(PPCContext& c, std::uint8_t* b) { Restore(24u, c, b); }
void OriginalSave27(PPCContext& c, std::uint8_t* b) { Save(27u, c, b); }
void OriginalRestore27(PPCContext& c, std::uint8_t* b) { Restore(27u, c, b); }
void OriginalSave28(PPCContext& c, std::uint8_t* b) { Save(28u, c, b); }
void OriginalRestore28(PPCContext& c, std::uint8_t* b) { Restore(28u, c, b); }

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-match-caller %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected ordinary-RAM and numeric/scalar paths; diagnostic continuation, other layouts, exceptions, faults, MMIO and runtime open");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
