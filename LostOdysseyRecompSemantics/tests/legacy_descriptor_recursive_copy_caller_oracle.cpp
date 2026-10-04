#include "lo_semantics/legacy_descriptor_recursive_copy_caller.h"
#include "lo_semantics/recovery_abi.h"
#include "semantic_oracle_support.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <stdexcept>

namespace
{
using namespace lo::semantic::gpu;
namespace family = legacy_descriptor_recursive_copy_caller;
constexpr GuestAddress Stack = 0x80000u, Owner = 0x10000u;
constexpr GuestAddress Name = 0x20000u, Node = 0x30000u;
constexpr GuestAddress Table = 0x35000u, Descriptor = 0x40020u;
constexpr GuestAddress Candidate = 0x42020u, Attachment = 0x50020u;
constexpr GuestAddress AttachmentOwner = 0x54000u, Metadata = 0x58000u;
constexpr GuestAddress Pool = 0x60000u, Previous = 0x70000u;
constexpr GuestAddress Input = 0x76000u, Child = 0x76100u;
constexpr GuestAddress List = 0x77000u, NextList = 0x78000u;
constexpr GuestAddress Sink = 0x79000u;
constexpr GuestAddress SelectorTable = 0x821712a0u;
constexpr GuestAddress JumpTable = 0x82174e48u;
constexpr GuestAddress FloatZero = 0x82000e50u;
constexpr std::array<test::Region, 4> Regions{{
    {0u, 0x90000u}, {0x82000000u, 0x1000u},
    {0x82171000u, 0x1000u}, {0x82174000u, 0x1000u}
}};
// The table bytes are from image_disc1.bin at guest 821712A0 and 82174E48.
constexpr std::array<std::uint8_t, 42> SelectorBytes{{
    0x2c, 0x34, 0x0c, 0x0c, 0x40, 0x0c, 0x0c, 0x40, 0x40, 0x40,
    0x3c, 0x40, 0x1c, 0x24, 0x40, 0x24, 0x24, 0x40, 0x40, 0x3c,
    0x14, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x14, 0x40,
    0x40, 0x40, 0x3c, 0x14, 0x40, 0x0c, 0x40, 0x40, 0x40, 0x40,
    0x14, 0x00
}};

constexpr std::array<std::uint8_t, 97> JumpBytes{{
    0x00, 0x00, 0x00, 0x00, 0xe8, 0x1c, 0x1c, 0xe8,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe0, 0xe0, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0x00,
    0x34, 0xe0, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe8, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x1c, 0x1c,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0x4c, 0x4c,
    0x4c, 0xe0, 0xe0, 0xe8, 0xe8, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x4c, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8, 0xe8,
    0x64, 0x64, 0xe8, 0xe8, 0xe8, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe8, 0x74, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0,
    0xe0
}};

enum class Path { OneValue, OneCopy, OneNested, TwoMismatch,
    TwoMatch, CursorWrap, Diagnostic };
struct Case
{
    const char* name;
    GuestAddress entry;
    unsigned count;
    Path path;
};
constexpr std::array Cases{
    Case{"one-value-copy", 0x83026e18u, 1u, Path::OneValue},
    Case{"one-attachment-copy", 0x83026e18u, 1u, Path::OneCopy},
    Case{"one-nested-copy", 0x83026e18u, 1u, Path::OneNested},
    Case{"two-mismatch", 0x83026e18u, 2u, Path::TwoMismatch},
    Case{"two-match", 0x83026e18u, 2u, Path::TwoMatch},
    Case{"iterator-wrap", 0x82ffd208u, 0u, Path::CursorWrap},
    Case{"diagnostic-frame", 0x82f99f48u, 0u, Path::Diagnostic}
};
unsigned allocate_calls = 0, copy_calls = 0, attach_calls = 0;
unsigned value_calls = 0, pool_calls = 0, recursive_calls = 0;
std::array<PPCRegister*, 32> Fields(PPCContext& c)
{
    return {&c.r0, &c.r1, &c.r2, &c.r3, &c.r4, &c.r5, &c.r6, &c.r7,
        &c.r8, &c.r9, &c.r10, &c.r11, &c.r12, &c.r13, &c.r14, &c.r15,
        &c.r16, &c.r17, &c.r18, &c.r19, &c.r20, &c.r21, &c.r22, &c.r23,
        &c.r24, &c.r25, &c.r26, &c.r27, &c.r28, &c.r29, &c.r30, &c.r31};
}
family::Registers FromPpc(PPCContext& c)
{
    family::Registers s{}; const auto fields = Fields(c);
    auto& g = s.integer;
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) g.r[i] = fields[i]->u64;
    g.sp = c.r1.u64; g.lr = c.lr; g.ctr = c.ctr.u64;
    g.xer_so = c.xer.so; g.xer_ca = c.xer.ca;
    g.cr0 = {c.cr0.lt, c.cr0.gt, c.cr0.eq, c.cr0.so};
    g.cr6 = {c.cr6.lt, c.cr6.gt, c.cr6.eq, c.cr6.so};
    s.f0_bits = c.f0.u64; s.f1_bits = c.f1.u64; s.f2_bits = c.f2.u64;
    s.f3_bits = c.f3.u64; s.f4_bits = c.f4.u64;
    s.f12_bits = c.f12.u64; s.f13_bits = c.f13.u64;
    s.cached_fp_control = c.fpscr.csr;
    return s;
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
        a.f13_bits == b.f13_bits &&
        a.cached_fp_control == b.cached_fp_control;
}

void ToPpc(PPCContext& c, const family::Registers& s)
{
    auto fields = Fields(c); const auto& g = s.integer;
    for (unsigned i = 0; i < 32u; ++i)
        if (i != 1u) fields[i]->u64 = g.r[i];
    c.r1.u64 = g.sp; c.lr = g.lr; c.ctr.u64 = g.ctr;
    c.xer.so = g.xer_so; c.xer.ca = g.xer_ca;
    c.cr0 = {g.cr0.lt, g.cr0.gt, g.cr0.eq, {g.cr0.so}};
    c.cr6 = {g.cr6.lt, g.cr6.gt, g.cr6.eq, {g.cr6.so}};
    c.f0.u64 = s.f0_bits; c.f1.u64 = s.f1_bits;
    c.f2.u64 = s.f2_bits; c.f3.u64 = s.f3_bits; c.f4.u64 = s.f4_bits;
    c.f12.u64 = s.f12_bits; c.f13.u64 = s.f13_bits;
    c.fpscr.csr = s.cached_fp_control;
}
PPCContext Initial(const Case& item)
{
    PPCContext c{}; auto fields = Fields(c);
    for (unsigned i = 0; i < 32u; ++i)
        fields[i]->u64 = 0x1122334400000000ull + i;
    c.r1.u64 = 0x1234567800000000ull | Stack;
    c.r3.u64 = Owner; c.r4.u64 = Input;
    c.r5.u64 = item.count; c.r6.u64 = 0u; c.r7.u64 = 0u;
    if (item.entry == 0x82ffd208u) c.r3.u64 = Input;
    if (item.entry == 0x82f99f48u) c.r4.u64 = 4800u;
    c.lr = 0xabcdef0123456789ull;
    c.ctr.u64 = 0x5566778899aabbccull;
    c.cr0.gt = 1; c.cr6.lt = 1; c.xer.so = 1; c.xer.ca = 1;
    c.f0.u64 = 0x4014000000000000ull;
    c.f1.u64 = 0x3ff0000000000000ull;
    c.f2.u64 = 0x4000000000000000ull;
    c.f3.u64 = 0x4008000000000000ull;
    c.f4.u64 = 0x4010000000000000ull;
    c.f12.u64 = 0x401c000000000000ull;
    c.f13.u64 = 0x4022000000000000ull;
    c.fpscr.csr = 0x1f80u;
    return c;
}
void Seed(test::GuestWindow& window, const Case& item)
{
    window.Fill(0xa5u);
    auto m = window.Memory();
    for (unsigned i = 0; i < SelectorBytes.size(); ++i)
        m.WriteU8(SelectorTable + i, SelectorBytes[i]);
    for (unsigned i = 0; i < JumpBytes.size(); ++i)
        m.WriteU8(JumpTable + i, JumpBytes[i]);
    m.WriteU32(FloatZero, 0u);
    m.WriteU32(Owner, 0u); m.WriteU32(Owner + 4u, Owner);
    m.WriteU32(Owner + 8u, 0u); m.WriteU32(Owner + 12u, Table);
    m.WriteU32(Owner + 16u, Previous); m.WriteU32(Previous, 0u);
    m.WriteU32(16u, Previous); m.WriteU32(Owner + 40u, 0u);
    m.WriteU32(Owner + 88u, 10u); m.WriteU32(Owner + 548u, Owner);
    m.WriteU32(Table + 4u, 4u);
    m.WriteU32(Owner + 912u, Pool + 4096u);
    m.WriteU32(Owner + 916u, Pool);
    m.WriteU32(Name + 24u, Node); m.WriteU32(Node, 0u);
    m.WriteU32(Descriptor, 0u); m.WriteU32(Descriptor + 8u, 58u << 7u);
    m.WriteU32(Descriptor + 24u, Name);
    m.WriteU32(Descriptor + 40u, Attachment);
    m.WriteU32(Attachment, 0x11223305u);
    m.WriteU32(Attachment + 4u, 0u);
    m.WriteU32(Attachment + 8u, 0x44556677u);
    m.WriteU32(Attachment + 12u, AttachmentOwner);
    m.WriteU32(Attachment + 16u, 0u);
    m.WriteU32(AttachmentOwner + 4u, 0u);
    m.WriteU32(0x50000u, Metadata);
    m.WriteU32(Metadata + 148u, Owner);
    constexpr std::uint32_t First = 0x3f800000u, Second = 0x40000000u;
    for (unsigned i = 0; i < 7u; ++i)
        m.WriteU32(Owner + 60u + i * 4u, Candidate);
    m.WriteU32(Candidate, 0u); m.WriteU32(Candidate + 4u, 0u);
    m.WriteU32(Candidate + 8u, 2u << 14u);
    m.WriteU32(Candidate + 16u, 0u); m.WriteU32(Candidate + 28u, 0u);
    m.WriteU32(Candidate + 40u, First); m.WriteU32(Candidate + 44u, Second);
    m.WriteU32(Input + 4u, List);
    m.WriteU32(Input + 8u, 0u);
    m.WriteU32(List + 4u, NextList);
    m.WriteU32(List + 8u, 3u);
    m.WriteU32(NextList + 4u, 0u);
    m.WriteU32(List + 16u,
        item.path == Path::OneCopy ? Attachment :
        item.path == Path::OneNested ? Child : Candidate);
    m.WriteU32(List + 20u,
        item.path == Path::OneCopy ? 2u :
        item.path == Path::OneNested ? 3u : 1u);
    m.WriteU32(List + 24u, Attachment);
    m.WriteU32(List + 28u, 2u);
    m.WriteU32(Child, Candidate); m.WriteU32(Child + 4u, 1u);
    if (item.path == Path::CursorWrap)
    {
        m.WriteU32(List + 8u, 1u);
        m.WriteU32(Input + 8u, 0u);
    }
}
struct Event
{
    GuestAddress target;
    std::uint64_t r3, r4, r5, r6, r7, sp, lr;
    bool operator==(const Event&) const = default;
};
struct Services final : legacy_descriptor_recursive_copy::GuestBoundaryServices,
    legacy_descriptor_array_allocation::Services
{
    Path path;
    std::array<Event, 16> events{};
    unsigned count = 0;
    const char* unexpected = nullptr;
    explicit Services(Path selected) : path(selected) {}
    void Call(GuestAddress entry, GuestMemory& m,
        legacy_descriptor_recursive_copy::Registers& state) override
    {
        const auto& g = state.integer;
        if (count < events.size()) events[count++] =
            {entry, g.r[3], g.r[4], g.r[5], g.r[6], g.r[7], g.sp, g.lr};
        else unexpected = "too many guest callbacks";
        if (entry == 0x83022948u)
        {
            state.integer.r[3] = 1u;
            state.integer.r[9] = 0x11223344000000e1ull;
        }
        else if (entry == 0x83026b90u)
        {
            state.integer.r[3] = path == Path::TwoMismatch &&
                g.r[4] == List + 24u ? 2u : 1u;
        }
        else if (entry == 0x82f99d98u)
        {
            state.integer.r[3] = 0x11223344000000e2ull;
            m.WriteU32(Sink, 0x82f99d98u);
        }
        else unexpected = "unselected guest caller path";
    }
    void AllocateFromPool(GuestMemory&,
        crt_stream_operations::Registers&) override
    { unexpected = "unselected exhausted guest pool"; }
    void SetHostFpControl(std::uint32_t control) override
    { simde_mm_setcsr(control); }
};
Services* active = nullptr;
GuestMemory* original_memory = nullptr;
void Check(const Case& item)
{
    std::fprintf(stderr, "recursive-caller %s original\n", item.name);
    std::fflush(stderr);
    test::GuestWindow original(Regions), recovered(Regions);
    Seed(original, item); Seed(recovered, item);
    auto left = original.Memory(), right = recovered.Memory();
    auto context = Initial(item);
    auto state = FromPpc(context);
    Services expected(item.path), actual(item.path);
    allocate_calls = copy_calls = attach_calls = value_calls = pool_calls = 0;
    recursive_calls = 0; active = &expected; original_memory = &left;
    const auto previous_csr = simde_mm_getcsr();
    simde_mm_setcsr(0x1f80u);
    if (item.entry == 0x83026e18u)
        __imp__sub_83026E18(context, original.Bytes());
    else if (item.entry == 0x82ffd208u)
        __imp__sub_82FFD208(context, original.Bytes());
    else __imp__sub_82F99F48(context, original.Bytes());
    const auto original_csr = simde_mm_getcsr();
    active = nullptr; original_memory = nullptr;
    if (expected.unexpected) throw std::runtime_error(expected.unexpected);
    const auto observed = FromPpc(context);
    if (item.path == Path::CursorWrap &&
        (left.ReadU32(Input + 4u) != NextList ||
            left.ReadU32(Input + 8u) != 0u))
        throw std::runtime_error("cursor wrap did not update iterator");
    if (item.path == Path::Diagnostic &&
        (expected.count != 1u ||
            expected.events[0].target != 0x82f99d98u ||
            left.ReadU32(Sink) != 0x82f99d98u))
        throw std::runtime_error("diagnostic tail path");
    std::fprintf(stderr, "recursive-caller %s recovered\n", item.name);
    std::fflush(stderr);
    simde_mm_setcsr(0x1f80u);
    if (!family::Apply(item.entry, right, {actual, actual}, state))
        throw std::runtime_error("recursive caller entry missing");
    const auto recovered_csr = simde_mm_getcsr();
    simde_mm_setcsr(previous_csr);
    if (actual.unexpected) throw std::runtime_error(actual.unexpected);
    bool same_events = expected.count == actual.count;
    if (same_events)
        for (unsigned i = 0; i < expected.count; ++i)
            same_events &= expected.events[i] == actual.events[i];
    if (!Same(observed, state) || original_csr != recovered_csr ||
        !same_events || !original.EqualCommitted(recovered))
    {
        for (unsigned i = 0; i < 32u; ++i)
            if (observed.integer.r[i] != state.integer.r[i])
                std::fprintf(stderr, "r%u %016llX/%016llX\n", i,
                    static_cast<unsigned long long>(observed.integer.r[i]),
                    static_cast<unsigned long long>(state.integer.r[i]));
        std::fprintf(stderr,
            "%s SP %llX/%llX LR %llX/%llX CTR %llX/%llX CR0 %u%u%u%u/%u%u%u%u CR6 %u%u%u%u/%u%u%u%u CA %u/%u F1 %llX/%llX CSR %08X/%08X events %u/%u\n",
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
            static_cast<unsigned long long>(observed.f1_bits),
            static_cast<unsigned long long>(state.f1_bits),
            original_csr, recovered_csr, expected.count, actual.count);
        for (auto region : Regions)
            for (std::size_t off = 0; off < region.size; ++off)
            {
                const auto address = std::size_t(region.base) + off;
                if (original.Bytes()[address] != recovered.Bytes()[address])
                {
                    std::fprintf(stderr, "RAM %08zX %02X/%02X\n",
                        address, original.Bytes()[address],
                        recovered.Bytes()[address]);
                    throw std::runtime_error(item.name);
                }
            }
        throw std::runtime_error(item.name);
    }
}
} // namespace
void OriginalGuestBoundary(GuestAddress entry, PPCContext& c, std::uint8_t*)
{
    auto state = FromPpc(c);
    active->Call(entry, *original_memory, state);
    ToPpc(c, state);
}
void OriginalBody82FFD208(PPCContext& c, std::uint8_t* b)
{ __imp__sub_82FFD208(c, b); }
void OriginalBody82F99F48(PPCContext& c, std::uint8_t* b)
{ __imp__sub_82F99F48(c, b); }
void OriginalBody83026C80(PPCContext& c, std::uint8_t* b)
{ ++recursive_calls; __imp__sub_83026C80(c, b); }
void OriginalBody83056B90(PPCContext& c, std::uint8_t* b)
{ ++allocate_calls; __imp__sub_83056B90(c, b); }
void OriginalBody82FB6B28(PPCContext& c, std::uint8_t* b)
{ ++copy_calls; __imp__sub_82FB6B28(c, b); }
void OriginalBody82FBDA20(PPCContext& c, std::uint8_t* b)
{ ++attach_calls; __imp__sub_82FBDA20(c, b); }
void OriginalBody82FBD850(PPCContext& c, std::uint8_t* b)
{ ++value_calls; __imp__sub_82FBD850(c, b); }
void OriginalBody82FB36B0(PPCContext& c, std::uint8_t* b)
{ ++pool_calls; __imp__sub_82FB36B0(c, b); }
void OriginalNativeAllocate(PPCContext&, std::uint8_t*) { std::abort(); }

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
        fields[i]->u64 = recovery_abi::ReadU64(
            m, c.r1.u32 - 8u * (33u - i));
    c.r12.u64 = m.ReadU32(c.r1.u32 - 8u); c.lr = c.r12.u64;
}
}
void OriginalSave23(PPCContext& c, std::uint8_t* b) { Save(23u, c, b); }
void OriginalRestore23(PPCContext& c, std::uint8_t* b) { Restore(23u, c, b); }
void OriginalSave25(PPCContext& c, std::uint8_t* b) { Save(25u, c, b); }
void OriginalRestore25(PPCContext& c, std::uint8_t* b) { Restore(25u, c, b); }
void OriginalSave27(PPCContext& c, std::uint8_t* b) { Save(27u, c, b); }
void OriginalRestore27(PPCContext& c, std::uint8_t* b) { Restore(27u, c, b); }
void OriginalSave28(PPCContext& c, std::uint8_t* b) { Save(28u, c, b); }
void OriginalRestore28(PPCContext& c, std::uint8_t* b) { Restore(28u, c, b); }
void OriginalSave29(PPCContext& c, std::uint8_t* b) { Save(29u, c, b); }
void OriginalRestore29(PPCContext& c, std::uint8_t* b) { Restore(29u, c, b); }

void OriginalSave19(PPCContext& c, std::uint8_t* b) { Save(19u, c, b); }
void OriginalRestore19(PPCContext& c, std::uint8_t* b) { Restore(19u, c, b); }
int main()
{
    try
    {
        for (const auto& item : Cases) Check(item);
        std::printf("PASS legacy-descriptor-recursive-copy-caller %zu actual PPC cases\n",
            Cases.size());
        std::puts("LIMIT selected iterator/descriptor layouts; 83026B90/83022948/83056CF8 and diagnostic 82F99D98 guest boundaries; no faults/MMIO/runtime acceptance");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
