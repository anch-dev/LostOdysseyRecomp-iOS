#include "lo_semantics/legacy_descriptor_owner_pool_callers.h"
#include "lo_semantics/memory_fill.h"
#include "lo_semantics/recovery_abi.h"
#include "legacy_config_format_dispatch_heap_fixture.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_owner_pool_callers;
constexpr GuestAddress Owner = 0x40000u, Arena = Owner + 772u;
constexpr GuestAddress Free = 0x50000u, Pool = 0x60000u;
constexpr GuestAddress GuestResult = 0x70000u, Stack = 0x80000u;
constexpr GuestAddress Sink = 0x78000u, Anchor = Arena + 152u;
constexpr std::array<test::Region, 3> Regions{{
    {0u, 0x90000u}, {0x83245000u, 0x2000u},
    {0x832d3000u, 0x1000u}
}};

enum class Path { Bump, Reuse, Exhausted, Large };
struct Case
{
    const char* name;
    GuestAddress entry;
    std::uint32_t bytes;
    Path path;
};
constexpr std::array Cases{
    Case{"bump-20", 0x82fd1240u, 20u, Path::Bump},
    Case{"bump-132-boundary", 0x82fd1240u, 132u, Path::Bump},
    Case{"reuse-20-fill", 0x82fd1240u, 20u, Path::Reuse},
    Case{"reuse-132-fill", 0x82fd1240u, 132u, Path::Reuse},
    Case{"exhausted-guest-20", 0x82fd1240u, 20u, Path::Exhausted},
    Case{"large-helper-2036", 0x82facbf0u, 2036u, Path::Large},
    Case{"large-owner-2036", 0x82fd1240u, 2036u, Path::Large}
};

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
    auto& g = s.integer;
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) g.r[i] = fields[i]->u64;
    g.sp = c.r1.u64; g.lr = c.lr; g.ctr = c.ctr.u64;
    g.xer_so = c.xer.so; g.xer_ca = c.xer.ca;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    s.f0_bits = c.f0.u64; s.f1_bits = c.f1.u64;
    s.f2_bits = c.f2.u64; s.f3_bits = c.f3.u64; s.f4_bits = c.f4.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.f30_bits = c.f30.u64; s.f31_bits = c.f31.u64;
    s.cached_fp_control = c.fpscr.csr;
    return s;
}
void ToPpc(PPCContext& c, const family::Registers& s)
{
    const auto fields = Fields(c);
    const auto& g = s.integer;
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) fields[i]->u64 = g.r[i];
    c.r1.u64 = g.sp; c.lr = g.lr; c.ctr.u64 = g.ctr;
    c.xer.so = g.xer_so; c.xer.ca = g.xer_ca;
    c.cr0 = {g.cr0.lt, g.cr0.gt, g.cr0.eq, {g.cr0.so}};
    c.cr6 = {g.cr6.lt, g.cr6.gt, g.cr6.eq, {g.cr6.so}};
    c.f0.u64 = s.f0_bits; c.f1.u64 = s.f1_bits;
    c.f2.u64 = s.f2_bits; c.f3.u64 = s.f3_bits; c.f4.u64 = s.f4_bits;
    c.f12.u64 = s.f12_bits; c.f13.u64 = s.f13_bits;
    c.f30.u64 = s.f30_bits; c.f31.u64 = s.f31_bits;
    c.fpscr.csr = s.cached_fp_control;
}
bool SameCondition(const crt_stream_operations::Condition& a,
    const crt_stream_operations::Condition& b)
{ return a.lt == b.lt && a.gt == b.gt && a.eq == b.eq && a.so == b.so; }
bool Same(const family::Registers& a, const family::Registers& b)
{
    const auto& x = a.integer; const auto& y = b.integer;
    return x.r == y.r && x.sp == y.sp && x.lr == y.lr && x.ctr == y.ctr &&
        x.xer_so == y.xer_so && x.xer_ca == y.xer_ca &&
        SameCondition(x.cr0, y.cr0) && SameCondition(x.cr6, y.cr6) &&
        a.f0_bits == b.f0_bits && a.f1_bits == b.f1_bits &&
        a.f2_bits == b.f2_bits && a.f3_bits == b.f3_bits &&
        a.f4_bits == b.f4_bits && a.f12_bits == b.f12_bits &&
        a.f13_bits == b.f13_bits && a.f30_bits == b.f30_bits &&
        a.f31_bits == b.f31_bits &&
        a.cached_fp_control == b.cached_fp_control;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{};
    const auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1122334400000000ull + i;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = item.entry == 0x82facbf0u ? Arena : Owner;
    c.r4.u64 = 0x5566778800000000ull | item.bytes;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x8877665544332211ull;
    c.xer.so = 1; c.xer.ca = 1; c.cr0.gt = 1; c.cr6.lt = 1;
    c.f0.u64 = 0x4008000000000000ull;
    c.f1.u64 = 0x4014000000000000ull;
    c.f2.u64 = 0x401c000000000000ull;
    c.f3.u64 = 0x4022000000000000ull;
    c.f4.u64 = 0x4026000000000000ull;
    c.f12.u64 = 0x402a000000000000ull;
    c.f13.u64 = 0x402e000000000000ull;
    c.f30.u64 = 0x4031000000000000ull;
    c.f31.u64 = 0x4033000000000000ull;
    c.fpscr.csr = 0x1f80u;
    return c;
}
void Seed(test::GuestWindow& window, const Case& item)
{
    window.Fill(0xa5u);
    auto m = window.Memory();
    m.WriteU32(Arena + 140u,
        item.path == Path::Bump ? Pool + 4096u : Pool);
    m.WriteU32(Arena + 144u,
        item.path == Path::Bump ? Pool : Pool + 4096u);
    const auto slot = Arena + item.bytes - 4u;
    if (item.path == Path::Reuse || item.path == Path::Exhausted)
        m.WriteU32(slot, item.path == Path::Reuse ? Free : 0u);
    if (item.path == Path::Reuse)
    {
        m.WriteU32(Free, 0u);
        for (unsigned i = 4u; i < item.bytes; ++i) m.WriteU8(Free + i, 0x5au);
    }
    if (item.path == Path::Large)
    {
        a8_heap_fixture::Seed(m);
        m.WriteU32(Anchor, Anchor);
    }
}
struct Event
{
    GuestAddress entry;
    std::uint64_t r3, r4, r5, sp, lr;
    bool operator==(const Event&) const = default;
};
struct Services final : family::GuestBoundaryServices,
    heap_allocation_context::BoundaryServices
{
    std::array<Event, 8> events{};
    unsigned count = 0;
    const char* unexpected = nullptr;
    void Add(GuestAddress entry, const family::Registers& s)
    {
        if (count < events.size()) events[count++] =
            {entry, s.integer.r[3], s.integer.r[4], s.integer.r[5],
                s.integer.sp, s.integer.lr};
        else unexpected = "too many guest callbacks";
    }
    void Call(GuestAddress entry, GuestMemory& m,
        family::Registers& s) override
    {
        Add(entry, s);
        if (entry != 0x82fac428u)
        { unexpected = "unselected guest allocation path"; return; }
        s.integer.r[3] = GuestResult;
        s.integer.r[9] = 0x11223344000000e9ull;
        s.f1_bits = 0x4042000000000000ull;
        m.WriteU32(Sink, 0x82fac428u);
    }
    void CallDirect(GuestAddress entry, GuestMemory& memory,
        heap_allocation_context::Registers& state) override
    {
        if (entry != 0x82b7bc40u)
        {
            std::fprintf(stderr, "unselected heap guest %08X\n", entry);
            unexpected = "unselected deeper guest heap path";
            return;
        }
        // 827CAD38 requests flag 8, so the accepted 823ACCB0 body invokes
        // 82B7BC40 to clear the payload. The original side runs its exact
        // pinned PPC body; this is the selected recovered context adapter.
        const auto destination = static_cast<GuestAddress>(state.r[3]);
        const auto bytes = static_cast<std::uint32_t>(state.r[5]);
        const auto padding = (0u - destination) & 3u;
        const auto prefix = bytes < padding ? bytes : padding;
        const auto remaining = bytes - prefix;
        (void)FillGuestMemory(memory, destination,
            static_cast<std::uint8_t>(state.r[4]), bytes);
        state.r[6] = state.r[3] + prefix + (remaining & ~3u);
        state.r[5] -= prefix;
        state.r[4] = (state.r[4] & 0xffffffff00000000ull) |
            (std::uint32_t{static_cast<std::uint8_t>(state.r[4])} *
                0x01010101u);
        state.r[0] = remaining & 3u;
        state.cr0 = {0u, std::uint8_t(state.r[0] != 0u),
            std::uint8_t(state.r[0] == 0u), state.xer_so};
        state.ctr = state.r[0] == 3u ? 1u : 0u;
    }
    void CallNative(GuestAddress, GuestMemory&,
        heap_allocation_context::Registers&) override
    { unexpected = "unselected kernel/RTL import"; }
};
Services* active = nullptr;
GuestMemory* original_memory = nullptr;

void Check(const Case& item)
{
    std::fprintf(stderr, "owner-pool %s original\n", item.name);
    std::fflush(stderr);
    test::GuestWindow original(Regions), recovered(Regions);
    Seed(original, item); Seed(recovered, item);
    auto left = original.Memory(), right = recovered.Memory();
    auto context = Initial(item);
    auto state = FromPpc(context);
    Services expected, actual;
    active = &expected; original_memory = &left;
    const auto previous_csr = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    if (item.entry == 0x82fd1240u)
        __imp__sub_82FD1240(context, original.Bytes());
    else __imp__sub_82FACBF0(context, original.Bytes());
    const auto original_csr = simde_mm_getcsr();
    active = nullptr; original_memory = nullptr;
    const auto observed = FromPpc(context);
    if (expected.unexpected) throw std::runtime_error(expected.unexpected);
    const auto expected_result = item.path == Path::Bump ? Pool :
        item.path == Path::Reuse ? Free :
        item.path == Path::Exhausted ? GuestResult :
        a8_heap_fixture::ExpectedAllocation() + 12u;
    if (observed.integer.r[3] != expected_result ||
        expected.count != (item.path == Path::Exhausted ? 1u : 0u))
    {
        std::fprintf(stderr, "%s original r3=%08X expected=%08X events=%u\n",
            item.name, static_cast<unsigned>(observed.integer.r[3]),
            expected_result, expected.count);
        throw std::runtime_error("actual owner-pool fixture path");
    }
    if (item.path == Path::Reuse &&
        (left.ReadU32(Arena + item.bytes - 4u) != 0u ||
            left.ReadU8(Free + item.bytes - 1u) != 0u))
        throw std::runtime_error("reuse pop/fill result");
    if (item.path == Path::Large &&
        (left.ReadU32(Anchor) != a8_heap_fixture::Payload + 4u ||
            left.ReadU32(a8_heap_fixture::Payload + 8u) != item.bytes + 12u))
        throw std::runtime_error("large owner-link result");
    std::fprintf(stderr, "owner-pool %s recovered\n", item.name);
    std::fflush(stderr);
    simde_mm_setcsr(0x1f80u);
    if (!family::Apply(item.entry, right, {actual, actual}, state))
        throw std::runtime_error("owner-pool entry missing");
    const auto recovered_csr = simde_mm_getcsr();
    simde_mm_setcsr(previous_csr);
    if (actual.unexpected) throw std::runtime_error(actual.unexpected);
    const bool same_events = expected.count == actual.count &&
        [&]{ for (unsigned i = 0; i < expected.count; ++i)
            if (!(expected.events[i] == actual.events[i])) return false;
            return true; }();
    if (!Same(observed, state) || original_csr != recovered_csr ||
        !same_events || !original.EqualCommitted(recovered))
    {
        for (unsigned i = 0; i < 32u; ++i)
            if (observed.integer.r[i] != state.integer.r[i])
                std::fprintf(stderr, "r%u %016llX/%016llX\n", i,
                    static_cast<unsigned long long>(observed.integer.r[i]),
                    static_cast<unsigned long long>(state.integer.r[i]));
        std::fprintf(stderr, "%s SP=%llX/%llX LR=%llX/%llX CTR=%llX/%llX "
            "CR0=%u%u%u%u/%u%u%u%u CR6=%u%u%u%u/%u%u%u%u "
            "CA=%u/%u SO=%u/%u F1=%llX/%llX F30=%llX/%llX "
            "F31=%llX/%llX CSR=%08X/%08X events=%u/%u\n",
            item.name,
            static_cast<unsigned long long>(observed.integer.sp),
            static_cast<unsigned long long>(state.integer.sp),
            static_cast<unsigned long long>(observed.integer.lr),
            static_cast<unsigned long long>(state.integer.lr),
            static_cast<unsigned long long>(observed.integer.ctr),
            static_cast<unsigned long long>(state.integer.ctr),
            observed.integer.cr0.lt, observed.integer.cr0.gt,
            observed.integer.cr0.eq, observed.integer.cr0.so,
            state.integer.cr0.lt, state.integer.cr0.gt,
            state.integer.cr0.eq, state.integer.cr0.so,
            observed.integer.cr6.lt, observed.integer.cr6.gt,
            observed.integer.cr6.eq, observed.integer.cr6.so,
            state.integer.cr6.lt, state.integer.cr6.gt,
            state.integer.cr6.eq, state.integer.cr6.so,
            observed.integer.xer_ca, state.integer.xer_ca,
            observed.integer.xer_so, state.integer.xer_so,
            static_cast<unsigned long long>(observed.f1_bits),
            static_cast<unsigned long long>(state.f1_bits),
            static_cast<unsigned long long>(observed.f30_bits),
            static_cast<unsigned long long>(state.f30_bits),
            static_cast<unsigned long long>(observed.f31_bits),
            static_cast<unsigned long long>(state.f31_bits),
            original_csr, recovered_csr, expected.count, actual.count);
        for (auto region : Regions)
            for (std::size_t offset = 0; offset < region.size; ++offset)
            {
                const auto address = std::size_t(region.base) + offset;
                if (original.Bytes()[address] != recovered.Bytes()[address])
                {
                    std::fprintf(stderr, "RAM %08zX %02X/%02X\n", address,
                        original.Bytes()[address], recovered.Bytes()[address]);
                    throw std::runtime_error(item.name);
                }
            }
        throw std::runtime_error(item.name);
    }
}

void Save(unsigned first, PPCContext& c, std::uint8_t* b)
{
    auto m = GuestMemory(0u,
        std::span<std::uint8_t>(b, test::GuestWindow::Space));
    auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        recovery_abi::WriteU64(m, static_cast<GuestAddress>(
            c.r1.u64 - 8u * (33u - i)), fields[i]->u64);
    m.WriteU32(static_cast<GuestAddress>(c.r1.u64 - 8u), c.r12.u32);
}
void Restore(unsigned first, PPCContext& c, std::uint8_t* b)
{
    auto m = GuestMemory(0u,
        std::span<std::uint8_t>(b, test::GuestWindow::Space));
    auto fields = Fields(c);
    for (unsigned i = first; i <= 31u; ++i)
        fields[i]->u64 = recovery_abi::ReadU64(m, static_cast<GuestAddress>(
            c.r1.u64 - 8u * (33u - i)));
    c.r12.u64 = m.ReadU32(static_cast<GuestAddress>(c.r1.u64 - 8u));
    c.lr = c.r12.u64;
}
} // namespace

void OriginalSave22(PPCContext& c, std::uint8_t* b) { Save(22u, c, b); }
void OriginalRestore22(PPCContext& c, std::uint8_t* b) { Restore(22u, c, b); }
void OriginalSave29(PPCContext& c, std::uint8_t* b) { Save(29u, c, b); }
void OriginalRestore29(PPCContext& c, std::uint8_t* b) { Restore(29u, c, b); }
void OriginalGuestBoundary(GuestAddress entry, PPCContext& c, std::uint8_t*)
{
    auto s = FromPpc(c);
    active->Call(entry, *original_memory, s);
    ToPpc(c, s);
}
void OriginalHeapBoundary(GuestAddress, PPCContext&, std::uint8_t*)
{ active->unexpected = "unselected deeper guest or kernel heap path"; }

int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-owner-pool-callers %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT 82FAC428 and physical heap branches retain mutable guest boundary; deeper heap guest callees and kernel imports remain selected; no faults, MMIO or runtime acceptance");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
